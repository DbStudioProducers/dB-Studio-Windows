package com.dbstudio.receiverbench

import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.media.*
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.*
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.math.PI
import kotlin.math.sin

/** Local stereo output test only. Network receiver is not implemented. */
class MainActivity : Activity() {
    private val bg = 0xFF101215.toInt()
    private val panel = 0xFF191D21.toInt()
    private val green = 0xFF43E638.toInt()
    private val muted = 0xFFA0A8AE.toInt()
    private lateinit var audioManager: AudioManager
    private lateinit var playButton: Button
    private lateinit var playback: TextView
    private lateinit var route: TextView
    private lateinit var frequencyLabel: TextView
    private var track: AudioTrack? = null
    private val running = AtomicBoolean(false)
    @Volatile private var frequency = 440
    @Volatile private var amplitude = 0.06

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        audioManager = getSystemService(Context.AUDIO_SERVICE) as AudioManager
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(22, 22, 22, 22)
            setBackgroundColor(bg)
        }
        root.addView(label("dB STUDIO", 27, Color.WHITE, true))
        root.addView(label("RECEIVER • ANDROID", 13, green, true))
        root.addView(label("Painel de avaliação funcional", 14, muted))
        root.addView(section("CONEXÃO"))
        root.addView(metric("Servidor", "Não conectado"))
        root.addView(metric("Canal seguro", "Não estabelecido"))
        root.addView(metric("AUX autorizado", "Indisponível"))
        root.addView(section("TESTE LOCAL DE ÁUDIO"))
        root.addView(label("Gera um tom estéreo no próprio celular. Não recebe áudio da mesa.", 13, muted))
        frequencyLabel = label("Frequência: 440 Hz", 15, Color.WHITE, true)
        root.addView(frequencyLabel)
        root.addView(SeekBar(this).apply {
            max = 19
            progress = 4
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(s: SeekBar?, p: Int, user: Boolean) {
                    frequency = 220 + p * 55
                    frequencyLabel.text = "Frequência: " + frequency + " Hz"
                }
                override fun onStartTrackingTouch(s: SeekBar?) {}
                override fun onStopTrackingTouch(s: SeekBar?) {}
            })
        })
        root.addView(label("Volume máximo limitado a 6%", 13, muted))
        root.addView(SeekBar(this).apply {
            max = 6
            progress = 3
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(s: SeekBar?, p: Int, user: Boolean) { amplitude = p / 100.0 }
                override fun onStartTrackingTouch(s: SeekBar?) {}
                override fun onStopTrackingTouch(s: SeekBar?) {}
            })
        })
        playButton = Button(this).apply {
            text = "Iniciar teste estéreo"
            setOnClickListener { if (running.get()) stopTone() else startTone() }
        }
        root.addView(playButton)
        playback = label("Reprodução: parada", 14, muted, true)
        root.addView(playback)
        route = label("Saída: verificando…", 13, muted)
        root.addView(route)
        root.addView(Button(this).apply {
            text = "Atualizar saídas"
            setOnClickListener { refreshRoute() }
        })
        root.addView(section("TRANSPORTE DE REDE"))
        root.addView(label(
            "QUIC v1/TLS 1.3, autenticação, autorização AUX e recepção da mesa ainda não implementados. Métricas de rede não são simuladas.",
            13, muted
        ))
        setContentView(root)
        refreshRoute()
    }

    private fun startTone() {
        if (running.get()) return
        val rate = 48000
        val min = AudioTrack.getMinBufferSize(rate, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT)
        if (min <= 0) { playback.text = "Saída indisponível: " + min; return }
        val frames = (min / 4).coerceAtLeast(1024)
        val local = AudioTrack.Builder()
            .setAudioAttributes(AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
            .setAudioFormat(AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                .setSampleRate(rate).setChannelMask(AudioFormat.CHANNEL_OUT_STEREO).build())
            .setBufferSizeInBytes(frames * 8)
            .setTransferMode(AudioTrack.MODE_STREAM).build()
        if (local.state != AudioTrack.STATE_INITIALIZED) {
            local.release(); playback.text = "Falha ao iniciar dispositivo de áudio"; return
        }
        track = local
        running.set(true)
        playButton.text = "Parar teste estéreo"
        playback.text = "Reprodução: teste local ativo"
        val worker = Thread {
            val pcm = ShortArray(frames * 2)
            var phase = 0.0
            while (running.get()) {
                val increment = 2.0 * PI * frequency / rate
                val peak = (amplitude * 32767).toInt()
                for (i in 0 until frames) {
                    val value = (sin(phase) * peak).toInt().toShort()
                    pcm[i * 2] = value
                    pcm[i * 2 + 1] = value
                    phase += increment
                    if (phase >= 2.0 * PI) phase -= 2.0 * PI
                }
                if (local.write(pcm, 0, pcm.size, AudioTrack.WRITE_BLOCKING) < 0) break
            }
            runOnUiThread {
                if (track === local) {
                    try { local.stop() } catch (_: IllegalStateException) {}
                    local.release()
                    track = null
                    running.set(false)
                    playButton.text = "Iniciar teste estéreo"
                    playback.text = "Reprodução: parada"
                }
            }
        }
        worker.name = "dBStudioTone"
        worker.start()
        local.play()
    }

    private fun stopTone() {
        running.set(false)
        try { track?.stop() } catch (_: IllegalStateException) {}
        playButton.text = "Iniciar teste estéreo"
        playback.text = "Reprodução: parada"
    }

    private fun refreshRoute() {
        val devices = audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)
        val found = devices.mapNotNull { d ->
            val type = when (d.type) {
                AudioDeviceInfo.TYPE_WIRED_HEADPHONES, AudioDeviceInfo.TYPE_WIRED_HEADSET -> "P2/fone com fio"
                AudioDeviceInfo.TYPE_USB_HEADSET, AudioDeviceInfo.TYPE_USB_DEVICE -> "USB"
                AudioDeviceInfo.TYPE_BUILTIN_SPEAKER -> "Alto-falante"
                AudioDeviceInfo.TYPE_BLUETOOTH_A2DP, AudioDeviceInfo.TYPE_BLE_HEADSET -> "Bluetooth"
                else -> null
            }
            if (type == null) null else type + " (id " + d.id + ")"
        }
        route.text = if (found.isEmpty()) "Saídas detectadas: nenhuma identificada" else
            "Saídas detectadas: " + found.joinToString(" • ")
    }

    override fun onDestroy() {
        stopTone()
        super.onDestroy()
    }

    private fun section(s: String) = TextView(this).apply {
        text = s; setTextColor(green); textSize = 13f; setPadding(0, 18, 0, 8)
    }
    private fun metric(name: String, value: String) = LinearLayout(this).apply {
        orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
        setPadding(14, 12, 14, 12); setBackgroundColor(panel)
        addView(label(name, 14, Color.WHITE), LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        addView(label(value, 14, muted, true).apply { gravity = Gravity.END },
            LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            .apply { bottomMargin = 5 }
    }
    private fun label(s: String, size: Int, color: Int, bold: Boolean = false) = TextView(this).apply {
        text = s; textSize = size.toFloat(); setTextColor(color)
        if (bold) setTypeface(typeface, android.graphics.Typeface.BOLD)
        setPadding(0, 5, 0, 5)
    }
}
