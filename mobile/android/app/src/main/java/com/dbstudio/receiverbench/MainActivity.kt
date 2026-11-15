package com.dbstudio.receiverbench

import android.app.Activity
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.TextView

/**
 * Instrumentation shell only. No network transport or audio playback is implemented yet.
 * Never display fabricated telemetry: unknown measurements remain unavailable.
 */
class MainActivity : Activity() {
    private val background = Color.rgb(16, 18, 21)
    private val panel = Color.rgb(25, 29, 33)
    private val accent = Color.rgb(67, 230, 56)
    private val muted = Color.rgb(160, 168, 174)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(24, 28, 24, 24)
            setBackgroundColor(background)
        }

        root.addView(label("dB STUDIO", 25, Color.WHITE, true))
        root.addView(label("RECEIVER BENCH  •  ANDROID", 12, accent, true))
        root.addView(label("Instrumento de validação — versão 0.1.0", 13, muted))
        root.addView(section("CONEXÃO"))
        root.addView(metric("Servidor", "Não conectado"))
        root.addView(metric("Canal seguro", "Não estabelecido"))
        root.addView(metric("AUX autorizado", "Indisponível"))
        root.addView(section("MÉTRICAS DE TRANSMISSÃO"))
        root.addView(metric("Pacotes recebidos / perdidos", "— / —"))
        root.addView(metric("Jitter / RTT", "— / —"))
        root.addView(metric("Pacotes fora de ordem", "—"))
        root.addView(metric("Underruns de áudio", "—"))
        root.addView(metric("Latência estimada", "—"))
        root.addView(section("SAÍDA DE ÁUDIO"))
        root.addView(metric("Rota ativa", "Ainda não medida"))
        root.addView(metric("Reprodução", "Não implementada"))
        root.addView(label(
            "ESTADO ATUAL: interface de instrumentação. Transporte QUIC, autenticação, " +
                "jitter buffer e reprodução ainda não estão conectados.",
            12, muted
        ))

        setContentView(root)
    }

    private fun section(text: String) = TextView(this).apply {
        this.text = text
        setTextColor(accent)
        textSize = 12f
        setPadding(0, 24, 0, 8)
    }

    private fun metric(name: String, value: String) = LinearLayout(this).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        setPadding(14, 12, 14, 12)
        setBackgroundColor(panel)
        val left = label(name, 14, Color.WHITE)
        val right = label(value, 14, muted, true).apply { gravity = Gravity.END }
        addView(left, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        addView(right, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        val params = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
        )
        params.bottomMargin = 5
        layoutParams = params
    }

    private fun label(text: String, size: Int, color: Int, bold: Boolean = false) =
        TextView(this).apply {
            this.text = text
            textSize = size.toFloat()
            setTextColor(color)
            if (bold) setTypeface(typeface, android.graphics.Typeface.BOLD)
            setPadding(0, 5, 0, 5)
        }
}
