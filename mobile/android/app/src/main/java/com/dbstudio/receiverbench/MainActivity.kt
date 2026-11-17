package com.dbstudio.receiverbench

import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.media.*
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.*
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.math.PI
import kotlin.math.sin

/**
 * Musician-screen layout prototype following LIVE_CONTROL_UI_SPEC.
 * Remote controls remain disabled until authenticated server transport exists.
 * Local tone is a device output diagnostic, not a received monitor mix.
 */
class MainActivity : Activity() {
    private val bg = 0xFF101215.toInt()
    private val panel = 0xFF191D21.toInt()
    private val panel2 = 0xFF252B30.toInt()
    private val green = 0xFF43E638.toInt()
    private val muted = 0xFFA0A8AE.toInt()
    private lateinit var audioManager: AudioManager
    private lateinit var playButton: Button
    private lateinit var playback: TextView
    private lateinit var route: TextView
    private lateinit var channelList: LinearLayout
    private lateinit var bankButtons: List<Button>
    private lateinit var bankStatus: TextView
    private var track: AudioTrack? = null
    private val running = AtomicBoolean(false)
    @Volatile private var frequency = 440
    @Volatile private var amplitude = 0.03

    private var selectedBank = 0
    private val channelNames = Array(32) { "Canal " + (it + 1).toString().padStart(2, '0') }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        audioManager = getSystemService(Context.AUDIO_SERVICE) as AudioManager
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(bg)
        }
        val header = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(18, 14, 18, 10)
            setBackgroundColor(bg)
        }
        header.addView(label("dB STUDIO", 26, Color.WHITE, true))
        header.addView(label("LIVE CONTROL  •  MUSICIAN", 12, green, true))
        val identity = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        identity.addView(label("Músico não autenticado", 14, Color.WHITE, true),
            LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        identity.addView(label("AUX —", 14, muted, true))
        header.addView(identity)
        header.addView(metric("CONEXÃO", "Desconectado • transporte indisponível"))
        root.addView(header)

        val scroll = ScrollView(this).apply { isFillViewport = true }
        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(16, 4, 16, 16)
            setBackgroundColor(bg)
        }
        content.addView(section("CANAIS"))
        val banks = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; weightSum = 4f }
        val buttons = ArrayList<Button>()
        val ranges = listOf("01–08", "09–16", "17–24", "25–32")
        for (i in 0..3) {
            val b = Button(this).apply {
                text = ranges[i]
                isAllCaps = false
                isEnabled = true
                setOnClickListener { selectBank(i) }
            }
            buttons.add(b)
            banks.addView(b, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f).apply {
                setMargins(2, 2, 2, 2)
            })
        }
        bankButtons = buttons
        content.addView(banks)
        bankStatus = label("BANCO 01–08", 12, green, true)
        content.addView(bankStatus)
        channelList = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        content.addView(channelList)
        content.addView(section("MASTER AUX"))
        content.addView(metric("Mix pessoal", "Aguardando atribuição do servidor"))
        val master = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(12, 10, 12, 10)
            setBackgroundColor(panel)
        }
        master.addView(label("Nível master", 14, Color.WHITE, true))
        master.addView(SeekBar(this).apply { max = 100; progress = 70; isEnabled = false })
        val masterButtons = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        masterButtons.addView(Button(this).apply { text = "MUTE"; isEnabled = false },
            LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        masterButtons.addView(Button(this).apply { text = "EQ"; isEnabled = false },
            LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        master.addView(masterButtons)
        content.addView(master)
        content.addView(section("SAÍDA DE ÁUDIO"))
        route = label("Saída: verificando…", 13, muted)
        content.addView(route)
        content.addView(Button(this).apply {
            text = "Atualizar saída"
            setOnClickListener { refreshRoute() }
        })
        content.addView(section("DIAGNÓSTICO LOCAL"))
        content.addView(label("O teste abaixo gera um tom no próprio telefone. Não é áudio recebido do Desktop Server.", 12, muted))
        val freqLabel = label("Frequência: 440 Hz", 13, Color.WHITE, true)
        content.addView(freqLabel)
        content.addView(SeekBar(this).apply {
            max = 19; progress = 4
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(s: SeekBar?, p: Int, user: Boolean) {
                    frequency = 220 + p * 55
                    freqLabel.text = "Frequência: " + frequency + " Hz"
                }
                override fun onStartTrackingTouch(s: SeekBar?) {}
                override fun onStopTrackingTouch(s: SeekBar?) {}
            })
        })
        content.addView(label("Nível máximo limitado a 3%", 12, muted))
        content.addView(SeekBar(this).apply {
            max = 3; progress = 2
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(s: SeekBar?, p: Int, user: Boolean) { amplitude = p / 100.0 }
                override fun onStartTrackingTouch(s: SeekBar?) {}
                override fun onStopTrackingTouch(s: SeekBar?) {}
            })
        })
        playButton = Button(this).apply {
            text = "Iniciar teste local estéreo"
            setOnClickListener { if (running.get()) stopTone() else startTone() }
        }
        content.addView(playButton)
        playback = label("Teste local: parado", 13, muted, true)
        content.addView(playback)
        content.addView(label(
            "Controles de mixagem estão bloqueados porque ainda não existe sessão QUIC autenticada. Nenhum comando remoto é enviado.",
            12, muted
        ))
        scroll.addView(content)
        root.addView(scroll, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f
        ))
        setContentView(root)
        selectBank(0)
        refreshRoute()
    }

    private fun selectBank(bank: Int) {
        selectedBank = bank.coerceIn(0, 3)
        val first = selectedBank * 8 + 1
        bankStatus.text = "BANCO " + first.toString().padStart(2, '0') + "–" + (first + 7).toString().padStart(2, '0')
        bankButtons.forEachIndexed { index, button ->
            button.setBackgroundColor(if (index == selectedBank) green else panel2)
            button.setTextColor(if (index == selectedBank) Color.BLACK else Color.WHITE)
        }
        channelList.removeAllViews()
        for (channel in first - 1 until first + 7) channelList.addView(channelStrip(channel))
    }

    private fun channelStrip(index: Int): View {
        val card = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(12, 8, 12, 8)
            setBackgroundColor(panel)
        }
        val heading = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        heading.addView(label(channelNames[index], 14, Color.WHITE, true),
            LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        heading.addView(label("SEM SINAL", 10, muted, true))
        card.addView(heading)
        card.addView(View(this).apply { setBackgroundColor(panel2) },
            LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 4).apply { topMargin = 5; bottomMargin = 5 })
        card.addView(SeekBar(this).apply { max = 100; progress = 70; isEnabled = false })
        val actions = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        actions.addView(Button(this).apply { text = "MUTE"; isEnabled = false },
            LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        actions.addView(Button(this).apply { text = "SOLO"; isEnabled = false },
            LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        card.addView(actions)
        card.layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            .apply { bottomMargin = 6 }
        return card
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
            .setBufferSizeInBytes(frames * 8).setTransferMode(AudioTrack.MODE_STREAM).build()
        if (local.state != AudioTrack.STATE_INITIALIZED) {
            local.release(); playback.text = "Falha ao inicializar saída"; return
        }
        track = local; running.set(true)
        playButton.text = "Parar teste local"
        playback.text = "Teste local: reproduzindo"
        val worker = Thread {
            val pcm = ShortArray(frames * 2)
            var phase = 0.0
            while (running.get()) {
                val increment = 2.0 * PI * frequency / rate
                val peak = (amplitude * 32767).toInt()
                for (i in 0 until frames) {
                    val value = (sin(phase) * peak).toInt().toShort()
                    pcm[i * 2] = value; pcm[i * 2 + 1] = value
                    phase += increment
                    if (phase >= 2.0 * PI) phase -= 2.0 * PI
                }
                if (local.write(pcm, 0, pcm.size, AudioTrack.WRITE_BLOCKING) < 0) break
            }
            runOnUiThread {
                if (track === local) {
                    try { local.stop() } catch (_: IllegalStateException) {}
                    local.release(); track = null; running.set(false)
                    playButton.text = "Iniciar teste local estéreo"
                    playback.text = "Teste local: parado"
                }
            }
        }
        worker.name = "dBStudioLocalTone"; worker.start(); local.play()
    }

    private fun stopTone() {
        running.set(false)
        try { track?.stop() } catch (_: IllegalStateException) {}
        playButton.text = "Iniciar teste local estéreo"
        playback.text = "Teste local: parado"
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
        route.text = if (found.isEmpty()) "Saídas detectadas: nenhuma identificada" else "Saídas detectadas: " + found.joinToString(" • ")
    }

    override fun onDestroy() {
        stopTone()
        super.onDestroy()
    }

    private fun section(s: String) = TextView(this).apply {
        text = s; setTextColor(green); textSize = 12f; setPadding(0, 16, 0, 7)
    }
    private fun metric(name: String, value: String) = LinearLayout(this).apply {
        orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
        setPadding(12, 9, 12, 9); setBackgroundColor(panel)
        addView(label(name, 12, Color.WHITE), LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        addView(label(value, 12, muted, true).apply { gravity = Gravity.END },
            LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            .apply { bottomMargin = 5 }
    }
    private fun label(s: String, size: Int, color: Int, bold: Boolean = false) = TextView(this).apply {
        text = s; textSize = size.toFloat(); setTextColor(color)
        if (bold) setTypeface(typeface, android.graphics.Typeface.BOLD)
        setPadding(0, 3, 0, 3)
    }
}
