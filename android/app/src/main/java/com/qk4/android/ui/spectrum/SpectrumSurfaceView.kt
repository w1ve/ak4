package com.qk4.android.ui.spectrum

import android.content.Context
import android.opengl.GLES30
import android.opengl.GLSurfaceView
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/**
 * OpenGL ES 3.0 SurfaceView for spectrum (panadapter + waterfall) rendering.
 * Requirement 6: GPU-Accelerated Spectrum Display.
 *
 * Touch gestures:
 * - Tap: tune to frequency at touch position
 * - Pinch: adjust span
 * - Horizontal drag: scroll center frequency
 */
class SpectrumSurfaceView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : GLSurfaceView(context, attrs) {

    interface SpectrumListener {
        fun onTouchTune(frequencyHz: Long)
        fun onSpanChange(newSpanHz: Int)
        fun onCenterScroll(deltaHz: Long)
    }

    var listener: SpectrumListener? = null
    var centerHz: Long = 14_225_000L
    var spanHz: Int = 100_000
    var refLevelDb: Int = 0

    private val renderer = SpectrumRenderer()
    private val scaleDetector: ScaleGestureDetector

    private var lastTouchX = 0f
    private var isDragging = false

    init {
        setEGLContextClientVersion(3)
        setRenderer(renderer)
        renderMode = RENDERMODE_CONTINUOUSLY

        scaleDetector = ScaleGestureDetector(context, object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
            override fun onScale(detector: ScaleGestureDetector): Boolean {
                val scaleFactor = detector.scaleFactor
                val newSpan = (spanHz / scaleFactor).toInt()
                listener?.onSpanChange(newSpan)
                return true
            }
        })
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        scaleDetector.onTouchEvent(event)

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                lastTouchX = event.x
                isDragging = false
            }
            MotionEvent.ACTION_MOVE -> {
                if (event.pointerCount == 1 && !scaleDetector.isInProgress) {
                    val dx = event.x - lastTouchX
                    if (Math.abs(dx) > 10) {
                        isDragging = true
                        // Convert pixel delta to frequency delta
                        val hzPerPixel = spanHz.toDouble() / width.toDouble()
                        val deltaHz = (-dx * hzPerPixel).toLong()
                        listener?.onCenterScroll(deltaHz)
                        lastTouchX = event.x
                    }
                }
            }
            MotionEvent.ACTION_UP -> {
                if (!isDragging && !scaleDetector.isInProgress) {
                    // Tap — touch-to-tune
                    val freq = pixelToFrequency(event.x)
                    listener?.onTouchTune(freq)
                }
            }
        }
        return true
    }

    /**
     * Update spectrum data from native layer.
     * Called from the JNI callback thread — queues for next GL frame.
     */
    fun updateSpectrumData(bins: FloatArray) {
        renderer.updateData(bins)
    }

    private fun pixelToFrequency(x: Float): Long {
        val ratio = x / width.toFloat()
        return centerHz - (spanHz / 2) + (ratio * spanHz).toLong()
    }

    /**
     * Internal OpenGL ES 3.0 renderer for spectrum trace + waterfall.
     */
    private inner class SpectrumRenderer : Renderer {

        private var spectrumProgram = 0
        private var waterfallProgram = 0
        private var vbo = 0
        private var spectrumData: FloatArray = FloatArray(0)
        private var dataUpdated = false
        private val dataLock = Object()

        // Waterfall texture (circular buffer)
        private var waterfallTexture = 0
        private var waterfallRow = 0
        private val waterfallRows = 480

        fun updateData(bins: FloatArray) {
            synchronized(dataLock) {
                spectrumData = bins.copyOf()
                dataUpdated = true
            }
        }

        override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
            GLES30.glClearColor(0.05f, 0.05f, 0.08f, 1.0f)

            // Compile shaders
            spectrumProgram = createProgram(SPECTRUM_VERTEX_SHADER, SPECTRUM_FRAGMENT_SHADER)
            waterfallProgram = createProgram(WATERFALL_VERTEX_SHADER, WATERFALL_FRAGMENT_SHADER)

            // Create VBO for spectrum line
            val buffers = IntArray(1)
            GLES30.glGenBuffers(1, buffers, 0)
            vbo = buffers[0]

            // Create waterfall texture
            val textures = IntArray(1)
            GLES30.glGenTextures(1, textures, 0)
            waterfallTexture = textures[0]
            GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, waterfallTexture)
            GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MIN_FILTER, GLES30.GL_LINEAR)
            GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MAG_FILTER, GLES30.GL_LINEAR)
            GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D, 0, GLES30.GL_R32F,
                4096, waterfallRows, 0, GLES30.GL_RED, GLES30.GL_FLOAT, null)
        }

        override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
            GLES30.glViewport(0, 0, width, height)
        }

        override fun onDrawFrame(gl: GL10?) {
            GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT)

            var data: FloatArray
            synchronized(dataLock) {
                if (!dataUpdated || spectrumData.isEmpty()) return
                data = spectrumData
                dataUpdated = false
            }

            val height = this@SpectrumSurfaceView.height
            val spectrumHeight = (height * 0.4f).toInt()
            val waterfallHeight = height - spectrumHeight

            // Draw spectrum trace in top 40%
            GLES30.glViewport(0, waterfallHeight, this@SpectrumSurfaceView.width, spectrumHeight)
            drawSpectrumTrace(data)

            // Draw waterfall in bottom 60%
            GLES30.glViewport(0, 0, this@SpectrumSurfaceView.width, waterfallHeight)
            updateWaterfallTexture(data)
            drawWaterfall()
        }

        private fun drawSpectrumTrace(data: FloatArray) {
            if (spectrumProgram == 0) return

            GLES30.glUseProgram(spectrumProgram)

            // Convert dB values to normalized Y coordinates
            val vertices = FloatArray(data.size * 2)
            for (i in data.indices) {
                vertices[i * 2] = i.toFloat() / (data.size - 1).toFloat() * 2f - 1f // X: -1 to 1
                val normalized = ((data[i] - refLevelDb + 80f) / 80f).coerceIn(0f, 1f)
                vertices[i * 2 + 1] = normalized * 2f - 1f // Y: -1 to 1
            }

            val buffer = ByteBuffer.allocateDirect(vertices.size * 4)
                .order(ByteOrder.nativeOrder())
                .asFloatBuffer()
                .put(vertices)
            buffer.position(0)

            GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, vbo)
            GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, vertices.size * 4, buffer, GLES30.GL_DYNAMIC_DRAW)

            val posLoc = GLES30.glGetAttribLocation(spectrumProgram, "aPosition")
            GLES30.glEnableVertexAttribArray(posLoc)
            GLES30.glVertexAttribPointer(posLoc, 2, GLES30.GL_FLOAT, false, 0, 0)

            GLES30.glLineWidth(2f)
            GLES30.glDrawArrays(GLES30.GL_LINE_STRIP, 0, data.size)

            GLES30.glDisableVertexAttribArray(posLoc)
        }

        private fun updateWaterfallTexture(data: FloatArray) {
            // Write current spectrum row into circular texture buffer
            val rowBuffer = ByteBuffer.allocateDirect(data.size * 4)
                .order(ByteOrder.nativeOrder())
                .asFloatBuffer()
                .put(data)
            rowBuffer.position(0)

            GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, waterfallTexture)
            GLES30.glTexSubImage2D(GLES30.GL_TEXTURE_2D, 0,
                0, waterfallRow, data.size, 1,
                GLES30.GL_RED, GLES30.GL_FLOAT, rowBuffer)

            waterfallRow = (waterfallRow + 1) % waterfallRows
        }

        private fun drawWaterfall() {
            if (waterfallProgram == 0) return

            GLES30.glUseProgram(waterfallProgram)

            val rowOffsetLoc = GLES30.glGetUniformLocation(waterfallProgram, "uRowOffset")
            GLES30.glUniform1f(rowOffsetLoc, waterfallRow.toFloat() / waterfallRows.toFloat())

            GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, waterfallTexture)

            // Draw fullscreen quad
            GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP, 0, 4)
        }

        private fun createProgram(vertexSrc: String, fragmentSrc: String): Int {
            val vs = compileShader(GLES30.GL_VERTEX_SHADER, vertexSrc)
            val fs = compileShader(GLES30.GL_FRAGMENT_SHADER, fragmentSrc)
            if (vs == 0 || fs == 0) return 0

            val program = GLES30.glCreateProgram()
            GLES30.glAttachShader(program, vs)
            GLES30.glAttachShader(program, fs)
            GLES30.glLinkProgram(program)

            val status = IntArray(1)
            GLES30.glGetProgramiv(program, GLES30.GL_LINK_STATUS, status, 0)
            if (status[0] == 0) {
                GLES30.glDeleteProgram(program)
                return 0
            }
            return program
        }

        private fun compileShader(type: Int, source: String): Int {
            val shader = GLES30.glCreateShader(type)
            GLES30.glShaderSource(shader, source)
            GLES30.glCompileShader(shader)

            val status = IntArray(1)
            GLES30.glGetShaderiv(shader, GLES30.GL_COMPILE_STATUS, status, 0)
            if (status[0] == 0) {
                GLES30.glDeleteShader(shader)
                return 0
            }
            return shader
        }
    }

    companion object {
        private const val SPECTRUM_VERTEX_SHADER = """
            #version 300 es
            in vec2 aPosition;
            void main() {
                gl_Position = vec4(aPosition, 0.0, 1.0);
            }
        """

        private const val SPECTRUM_FRAGMENT_SHADER = """
            #version 300 es
            precision mediump float;
            out vec4 fragColor;
            void main() {
                fragColor = vec4(0.2, 0.8, 0.4, 1.0);
            }
        """

        private const val WATERFALL_VERTEX_SHADER = """
            #version 300 es
            out vec2 vTexCoord;
            void main() {
                vec2 positions[4] = vec2[](
                    vec2(-1.0, -1.0),
                    vec2( 1.0, -1.0),
                    vec2(-1.0,  1.0),
                    vec2( 1.0,  1.0)
                );
                vec2 texCoords[4] = vec2[](
                    vec2(0.0, 0.0),
                    vec2(1.0, 0.0),
                    vec2(0.0, 1.0),
                    vec2(1.0, 1.0)
                );
                gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
                vTexCoord = texCoords[gl_VertexID];
            }
        """

        private const val WATERFALL_FRAGMENT_SHADER = """
            #version 300 es
            precision mediump float;
            in vec2 vTexCoord;
            out vec4 fragColor;
            uniform sampler2D uTexture;
            uniform float uRowOffset;
            void main() {
                vec2 tc = vTexCoord;
                tc.y = fract(tc.y + uRowOffset);
                float val = texture(uTexture, tc).r;
                // Color map: blue → green → yellow → red
                float normalized = clamp((val + 120.0) / 80.0, 0.0, 1.0);
                vec3 color;
                if (normalized < 0.33) {
                    color = mix(vec3(0.0, 0.0, 0.2), vec3(0.0, 0.5, 0.0), normalized * 3.0);
                } else if (normalized < 0.66) {
                    color = mix(vec3(0.0, 0.5, 0.0), vec3(1.0, 1.0, 0.0), (normalized - 0.33) * 3.0);
                } else {
                    color = mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (normalized - 0.66) * 3.0);
                }
                fragColor = vec4(color, 1.0);
            }
        """
    }
}
