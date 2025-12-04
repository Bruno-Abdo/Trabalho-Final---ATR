"""
ui/dashboard.py

Dashboard animado de monitoramento do caminhão autônomo.
Exibe métricas em tempo real com animações suaves.
"""

from __future__ import annotations

from typing import Dict, Any, List
from collections import deque

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QLabel, QFrame, QProgressBar, QGroupBox
)
from PyQt6.QtCore import Qt, QTimer, QDateTime, QPropertyAnimation, QEasingCurve, pyqtProperty
from PyQt6.QtGui import QFont, QColor, QPalette


class AnimatedCard(QFrame):
    """
    Card animado para exibir métricas individuais.
    Suporta animação de valor e mudança de cor de fundo.
    """
    
    def __init__(self, title: str, unit: str = "", parent=None):
        super().__init__(parent)
        self._value = 0.0
        self._target_value = 0.0
        self.unit = unit
        
        self.setFrameShape(QFrame.Shape.StyledPanel)
        self.setStyleSheet("""
            AnimatedCard {
                background-color: white;
                border: 1px solid #dee2e6;
                border-radius: 8px;
                padding: 15px;
            }
            AnimatedCard:hover {
                border: 1px solid #007bff;
                box-shadow: 0 2px 8px rgba(0,0,0,0.1);
            }
        """)
        
        layout = QVBoxLayout(self)
        layout.setSpacing(10)
        
        # Título
        self.lbl_title = QLabel(title)
        self.lbl_title.setStyleSheet("""
            font-size: 11pt;
            color: #6c757d;
            font-weight: 500;
        """)
        layout.addWidget(self.lbl_title)
        
        # Valor principal
        self.lbl_value = QLabel("--")
        self.lbl_value.setStyleSheet("""
            font-size: 24pt;
            font-weight: bold;
            color: #212529;
        """)
        self.lbl_value.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(self.lbl_value)
        
        # Animação de valor
        self.animation = QPropertyAnimation(self, b"value")
        self.animation.setDuration(500)  # 500ms
        self.animation.setEasingCurve(QEasingCurve.Type.OutCubic)
    
    @pyqtProperty(float)
    def value(self):
        return self._value
    
    @value.setter
    def value(self, val):
        self._value = val
        if self.unit:
            self.lbl_value.setText(f"{val:.1f}{self.unit}")
        else:
            self.lbl_value.setText(f"{val:.1f}")
    
    def set_value(self, value: float, animate: bool = True):
        """Define novo valor com animação opcional."""
        if animate:
            self.animation.setStartValue(self._value)
            self.animation.setEndValue(value)
            self.animation.start()
        else:
            self.value = value
    
    def set_color(self, color: str):
        """Altera cor do valor."""
        self.lbl_value.setStyleSheet(f"""
            font-size: 24pt;
            font-weight: bold;
            color: {color};
        """)


class TemperatureCard(AnimatedCard):
    """Card especializado para temperatura com indicador de alerta."""
    
    def __init__(self, parent=None):
        super().__init__("Temperatura do Motor", "°C", parent)
        
        # Barra de progresso visual
        self.progress = QProgressBar()
        self.progress.setRange(-100, 200)
        self.progress.setTextVisible(False)
        self.progress.setMaximumHeight(8)
        self.progress.setStyleSheet("""
            QProgressBar {
                border: none;
                border-radius: 4px;
                background-color: #e9ecef;
            }
            QProgressBar::chunk {
                border-radius: 4px;
                background-color: #28a745;
            }
        """)
        self.layout().addWidget(self.progress)
        
        # Status
        self.lbl_status = QLabel("Normal")
        self.lbl_status.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.lbl_status.setStyleSheet("font-size: 9pt; color: #28a745;")
        self.layout().addWidget(self.lbl_status)
    
    def set_value(self, value: float, animate: bool = True):
        """Atualiza temperatura com código de cores."""
        super().set_value(value, animate)
        self.progress.setValue(int(value))
        
        # Atualiza cor baseado em limites (Tabela 1)
        if value >= 120:
            self.set_color("#dc3545")
            self.lbl_status.setText("DEFEITO")
            self.lbl_status.setStyleSheet("font-size: 9pt; color: #dc3545; font-weight: bold;")
            self.progress.setStyleSheet("""
                QProgressBar { border: none; border-radius: 4px; background-color: #e9ecef; }
                QProgressBar::chunk { border-radius: 4px; background-color: #dc3545; }
            """)
        elif value >= 95:
            self.set_color("#ffc107")
            self.lbl_status.setText("ALERTA")
            self.lbl_status.setStyleSheet("font-size: 9pt; color: #ffc107; font-weight: bold;")
            self.progress.setStyleSheet("""
                QProgressBar { border: none; border-radius: 4px; background-color: #e9ecef; }
                QProgressBar::chunk { border-radius: 4px; background-color: #ffc107; }
            """)
        else:
            self.set_color("#28a745")
            self.lbl_status.setText("Normal")
            self.lbl_status.setStyleSheet("font-size: 9pt; color: #28a745;")
            self.progress.setStyleSheet("""
                QProgressBar { border: none; border-radius: 4px; background-color: #e9ecef; }
                QProgressBar::chunk { border-radius: 4px; background-color: #28a745; }
            """)


class FaultCard(QFrame):
    """Card para exibir status de falhas."""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        
        self.setFrameShape(QFrame.Shape.StyledPanel)
        self.setStyleSheet("""
            FaultCard {
                background-color: white;
                border: 1px solid #dee2e6;
                border-radius: 8px;
                padding: 15px;
            }
        """)
        
        layout = QVBoxLayout(self)
        layout.setSpacing(10)
        
        # Título
        title = QLabel("Status de Falhas")
        title.setStyleSheet("font-size: 11pt; color: #6c757d; font-weight: 500;")
        layout.addWidget(title)
        
        # Falha elétrica
        self.fault_electric_frame = QFrame()
        self.fault_electric_frame.setStyleSheet("""
            background-color: #d1ecf1;
            border-left: 4px solid #17a2b8;
            border-radius: 4px;
            padding: 8px;
        """)
        electric_layout = QHBoxLayout(self.fault_electric_frame)
        electric_layout.setContentsMargins(8, 5, 8, 5)
        
        self.lbl_electric = QLabel("Elétrica: OK")
        self.lbl_electric.setStyleSheet("font-size: 10pt; color: #0c5460;")
        electric_layout.addWidget(self.lbl_electric)
        layout.addWidget(self.fault_electric_frame)
        
        # Falha hidráulica
        self.fault_hydraulic_frame = QFrame()
        self.fault_hydraulic_frame.setStyleSheet("""
            background-color: #d1ecf1;
            border-left: 4px solid #17a2b8;
            border-radius: 4px;
            padding: 8px;
        """)
        hydraulic_layout = QHBoxLayout(self.fault_hydraulic_frame)
        hydraulic_layout.setContentsMargins(8, 5, 8, 5)
        
        self.lbl_hydraulic = QLabel("Hidráulica: OK")
        self.lbl_hydraulic.setStyleSheet("font-size: 10pt; color: #0c5460;")
        hydraulic_layout.addWidget(self.lbl_hydraulic)
        layout.addWidget(self.fault_hydraulic_frame)
        
        layout.addStretch()
    
    def update_faults(self, electric: bool, hydraulic: bool):
        """Atualiza status das falhas."""
        # Falha elétrica
        if electric:
            self.lbl_electric.setText("Elétrica: FALHA")
            self.fault_electric_frame.setStyleSheet("""
                background-color: #f8d7da;
                border-left: 4px solid #dc3545;
                border-radius: 4px;
                padding: 8px;
            """)
            self.lbl_electric.setStyleSheet("font-size: 10pt; color: #721c24; font-weight: bold;")
        else:
            self.lbl_electric.setText("Elétrica: OK")
            self.fault_electric_frame.setStyleSheet("""
                background-color: #d1ecf1;
                border-left: 4px solid #17a2b8;
                border-radius: 4px;
                padding: 8px;
            """)
            self.lbl_electric.setStyleSheet("font-size: 10pt; color: #0c5460;")
        
        # Falha hidráulica
        if hydraulic:
            self.lbl_hydraulic.setText("Hidráulica: FALHA")
            self.fault_hydraulic_frame.setStyleSheet("""
                background-color: #f8d7da;
                border-left: 4px solid #dc3545;
                border-radius: 4px;
                padding: 8px;
            """)
            self.lbl_hydraulic.setStyleSheet("font-size: 10pt; color: #721c24; font-weight: bold;")
        else:
            self.lbl_hydraulic.setText("Hidráulica: OK")
            self.fault_hydraulic_frame.setStyleSheet("""
                background-color: #d1ecf1;
                border-left: 4px solid #17a2b8;
                border-radius: 4px;
                padding: 8px;
            """)
            self.lbl_hydraulic.setStyleSheet("font-size: 10pt; color: #0c5460;")


class DashboardWidget(QWidget):
    """
    Widget principal do Dashboard.
    Substitui o placeholder na MainWindow.
    """
    
    def __init__(self, parent=None):
        super().__init__(parent)

        # === CONTROLE DE TAXA DE ATUALIZAÇÃO ===
        self._update_interval_ms = 100  # Atualiza no máximo a cada 200ms (5 FPS)
        self._last_update_time = 0
        self._pending_data = None  # Armazena dados pendentes
        
        # Timer para atualização throttled
        self._update_timer = QTimer(self)
        self._update_timer.timeout.connect(self._process_pending_update)
        self._update_timer.setSingleShot(True)
        self._setup_ui()
    
    def _setup_ui(self):
        """Configura layout do dashboard."""
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(15, 15, 15, 15)
        main_layout.setSpacing(15)
        
        # === ROW 1: Posição e Ângulo ===
        row1_layout = QHBoxLayout()
        row1_layout.setSpacing(15)
        
        self.card_pos_x = AnimatedCard("Posição X", " m")
        row1_layout.addWidget(self.card_pos_x)
        
        self.card_pos_y = AnimatedCard("Posição Y", " m")
        row1_layout.addWidget(self.card_pos_y)
        
        self.card_angle = AnimatedCard("Ângulo", "°")
        row1_layout.addWidget(self.card_angle)
        
        main_layout.addLayout(row1_layout)
        
        # === ROW 2: Temperatura e Falhas ===
        row2_layout = QHBoxLayout()
        row2_layout.setSpacing(15)
        
        self.card_temp = TemperatureCard()
        row2_layout.addWidget(self.card_temp, 2)
        
        self.card_faults = FaultCard()
        row2_layout.addWidget(self.card_faults, 1)
        
        main_layout.addLayout(row2_layout)
        
        # === ROW 3: Atuadores ===
        actuators_group = QGroupBox("Comandos dos Atuadores")
        actuators_group.setStyleSheet("""
            QGroupBox {
                font-size: 11pt;
                font-weight: bold;
                color: #495057;
                border: 1px solid #dee2e6;
                border-radius: 8px;
                margin-top: 10px;
                padding-top: 10px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
            }
        """)
        actuators_layout = QHBoxLayout(actuators_group)
        
        self.card_accel = AnimatedCard("Aceleração", "%")
        actuators_layout.addWidget(self.card_accel)
        
        self.card_steer = AnimatedCard("Direção", "°")
        actuators_layout.addWidget(self.card_steer)
        
        main_layout.addWidget(actuators_group)
        
        main_layout.addStretch()
    
    def update_data(self, state: Any, sensors: Dict[str, Any]):
        """
        Atualiza todos os cards com novos dados (THROTTLED).
        Limita atualizações para evitar sobrecarga visual.
        """
        from PyQt6.QtCore import QDateTime
        
        current_time = QDateTime.currentMSecsSinceEpoch()
        time_since_last_update = current_time - self._last_update_time
        
        # Se passou tempo suficiente, atualiza imediatamente
        if time_since_last_update >= self._update_interval_ms:
            self._apply_update(state, sensors)
            self._last_update_time = current_time
            self._pending_data = None
        else:
            # Armazena dados e agenda atualização futura
            self._pending_data = (state, sensors)
            remaining_time = self._update_interval_ms - time_since_last_update
            
            if not self._update_timer.isActive():
                self._update_timer.start(int(remaining_time))       

    def _process_pending_update(self):
        """Processa atualização pendente agendada pelo timer."""
        if self._pending_data is not None:
            state, sensors = self._pending_data
            self._apply_update(state, sensors)
            self._last_update_time = QDateTime.currentMSecsSinceEpoch()
            self._pending_data = None
    
    def _apply_update(self, state: Any, sensors: Dict[str, Any]):
        """Aplica atualização real aos widgets."""
        # Posição e ângulo
        self.card_pos_x.set_value(sensors.get("i_posicao_x", 0))
        self.card_pos_y.set_value(sensors.get("i_posicao_y", 0))
        self.card_angle.set_value(sensors.get("i_angulo_x", 0))
        
        # Temperatura
        temp = sensors.get("i_temperatura", 0)
        self.card_temp.set_value(temp)
        
        # Falhas
        self.card_faults.update_faults(
            sensors.get("i_falha_eletrica", False),
            sensors.get("i_falha_hidraulica", False)
        )
        
        # Atuadores
        self.card_accel.set_value(state.o_aceleracao if hasattr(state, 'o_aceleracao') else 0)
        self.card_steer.set_value(state.o_direcao if hasattr(state, 'o_direcao') else 0)