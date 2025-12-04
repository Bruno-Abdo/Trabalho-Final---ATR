"""
ui/faults_panel.py

Painel de controle para injeção de falhas no sistema de simulação.
Permite ao usuário ativar/desativar falhas em tempo real.
"""

from __future__ import annotations

import logging
from typing import TYPE_CHECKING

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox,
    QPushButton, QCheckBox, QLabel, QSlider, QFrame
)
from PyQt6.QtCore import Qt, pyqtSignal

if TYPE_CHECKING:
    from model.faults import FaultInjector

logger = logging.getLogger(__name__)


class FaultsPanel(QWidget):
    """
    Painel de controle de injeção de falhas.
    
    Emite sinais quando falhas são ativadas/desativadas,
    permitindo integração com o Simulator.
    """
    
    # Sinais emitidos quando falhas mudam
    electric_fault_changed = pyqtSignal(bool)
    hydraulic_fault_changed = pyqtSignal(bool)
    temperature_overheat_changed = pyqtSignal(bool, float)
    temperature_warning_changed = pyqtSignal(bool, float)
    reset_all_requested = pyqtSignal()
    
    def __init__(self, fault_injector: FaultInjector, parent=None):
        super().__init__(parent)
        self.fault_injector = fault_injector
        self._setup_ui()
        self._connect_signals()
    
    def _setup_ui(self):
        """Configura interface do painel."""
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(20, 20, 20, 20)
        main_layout.setSpacing(20)
        
        # Título
        title = QLabel("Injeção de Falhas")
        title.setStyleSheet("""
            font-size: 18pt;
            font-weight: bold;
            color: #495057;
            padding-bottom: 10px;
        """)
        main_layout.addWidget(title)
        
        # Descrição
        desc = QLabel(
            "Use os controles abaixo para simular falhas no sistema. "
            "As falhas serão aplicadas em tempo real durante a simulação."
        )
        desc.setWordWrap(True)
        desc.setStyleSheet("color: #6c757d; font-size: 10pt;")
        main_layout.addWidget(desc)
        
        # === GRUPO 1: Falhas de Sistema ===
        system_faults_group = QGroupBox("Falhas de Sistema")
        system_faults_group.setStyleSheet("""
            QGroupBox {
                font-size: 12pt;
                font-weight: bold;
                color: #495057;
                border: 2px solid #dee2e6;
                border-radius: 8px;
                margin-top: 15px;
                padding-top: 15px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 15px;
                padding: 0 8px;
            }
        """)
        system_layout = QVBoxLayout(system_faults_group)
        system_layout.setSpacing(15)
        
        # Checkbox: Falha Elétrica
        self.chk_electric = QCheckBox("Falha Elétrica")
        self.chk_electric.setStyleSheet("""
            QCheckBox {
                font-size: 11pt;
                padding: 8px;
            }
            QCheckBox::indicator {
                width: 20px;
                height: 20px;
            }
        """)
        system_layout.addWidget(self.chk_electric)
        
        # Checkbox: Falha Hidráulica
        self.chk_hydraulic = QCheckBox("Falha Hidráulica")
        self.chk_hydraulic.setStyleSheet("""
            QCheckBox {
                font-size: 11pt;
                padding: 8px;
            }
            QCheckBox::indicator {
                width: 20px;
                height: 20px;
            }
        """)
        system_layout.addWidget(self.chk_hydraulic)
        
        main_layout.addWidget(system_faults_group)
        
        # === GRUPO 2: Falhas de Temperatura ===
        temp_faults_group = QGroupBox("Controle de Temperatura")
        temp_faults_group.setStyleSheet("""
            QGroupBox {
                font-size: 12pt;
                font-weight: bold;
                color: #495057;
                border: 2px solid #dee2e6;
                border-radius: 8px;
                margin-top: 15px;
                padding-top: 15px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 15px;
                padding: 0 8px;
            }
        """)
        temp_layout = QVBoxLayout(temp_faults_group)
        temp_layout.setSpacing(15)
        
        # Checkbox: Alerta (95-119°C)
        self.chk_temp_warning = QCheckBox("Temperatura de Alerta (>95°C)")
        self.chk_temp_warning.setStyleSheet("""
            QCheckBox {
                font-size: 11pt;
                padding: 8px;
            }
            QCheckBox::indicator {
                width: 20px;
                height: 20px;
            }
        """)
        temp_layout.addWidget(self.chk_temp_warning)
        
        # Slider para temperatura de alerta
        warning_slider_layout = QHBoxLayout()
        warning_slider_layout.setContentsMargins(30, 0, 10, 0)
        
        self.slider_warning = QSlider(Qt.Orientation.Horizontal)
        self.slider_warning.setRange(95, 119)
        self.slider_warning.setValue(100)
        self.slider_warning.setEnabled(False)
        warning_slider_layout.addWidget(self.slider_warning)
        
        self.lbl_warning_temp = QLabel("100°C")
        self.lbl_warning_temp.setStyleSheet("font-weight: bold; min-width: 60px;")
        warning_slider_layout.addWidget(self.lbl_warning_temp)
        
        temp_layout.addLayout(warning_slider_layout)
        
        # Checkbox: Defeito (>120°C)
        self.chk_temp_overheat = QCheckBox("Superaquecimento/Defeito (>120°C)")
        self.chk_temp_overheat.setStyleSheet("""
            QCheckBox {
                font-size: 11pt;
                padding: 8px;
            }
            QCheckBox::indicator {
                width: 20px;
                height: 20px;
            }
        """)
        temp_layout.addWidget(self.chk_temp_overheat)
        
        # Slider para temperatura de defeito
        overheat_slider_layout = QHBoxLayout()
        overheat_slider_layout.setContentsMargins(30, 0, 10, 0)
        
        self.slider_overheat = QSlider(Qt.Orientation.Horizontal)
        self.slider_overheat.setRange(120, 200)
        self.slider_overheat.setValue(125)
        self.slider_overheat.setEnabled(False)
        overheat_slider_layout.addWidget(self.slider_overheat)
        
        self.lbl_overheat_temp = QLabel("125°C")
        self.lbl_overheat_temp.setStyleSheet("font-weight: bold; min-width: 60px;")
        overheat_slider_layout.addWidget(self.lbl_overheat_temp)
        
        temp_layout.addLayout(overheat_slider_layout)
        
        main_layout.addWidget(temp_faults_group)
        
        # === BOTÃO DE RESET ===
        reset_frame = QFrame()
        reset_frame.setStyleSheet("""
            QFrame {
                background-color: #f8f9fa;
                border: 1px solid #dee2e6;
                border-radius: 8px;
                padding: 15px;
            }
        """)
        reset_layout = QVBoxLayout(reset_frame)
        
        reset_label = QLabel("Resetar Todas as Falhas")
        reset_label.setStyleSheet("font-weight: bold; font-size: 11pt; color: #495057;")
        reset_layout.addWidget(reset_label)
        
        reset_desc = QLabel(
            "Remove todas as falhas ativas e retorna o sistema ao estado normal."
        )
        reset_desc.setWordWrap(True)
        reset_desc.setStyleSheet("color: #6c757d; font-size: 9pt;")
        reset_layout.addWidget(reset_desc)
        
        self.btn_reset = QPushButton("Resetar Falhas")
        self.btn_reset.setMinimumHeight(40)
        self.btn_reset.setStyleSheet("""
            QPushButton {
                background-color: #17a2b8;
                color: white;
                font-weight: bold;
                font-size: 11pt;
                padding: 10px;
                border: none;
                border-radius: 6px;
            }
            QPushButton:hover {
                background-color: #138496;
            }
            QPushButton:pressed {
                background-color: #117a8b;
            }
        """)
        reset_layout.addWidget(self.btn_reset)
        
        main_layout.addWidget(reset_frame)
        
        main_layout.addStretch()
    
    def _connect_signals(self):
        """Conecta sinais internos."""
        # Falhas de sistema
        self.chk_electric.toggled.connect(self._on_electric_toggled)
        self.chk_hydraulic.toggled.connect(self._on_hydraulic_toggled)
        
        # Temperatura
        self.chk_temp_warning.toggled.connect(self._on_warning_toggled)
        self.chk_temp_overheat.toggled.connect(self._on_overheat_toggled)
        
        self.slider_warning.valueChanged.connect(self._on_warning_slider_changed)
        self.slider_overheat.valueChanged.connect(self._on_overheat_slider_changed)
        
        # Reset
        self.btn_reset.clicked.connect(self._on_reset_clicked)
    
    # ========================================================================
    # Handlers de Eventos
    # ========================================================================
    
    def _on_electric_toggled(self, checked: bool):
        """Handler do checkbox de falha elétrica."""
        self.fault_injector.set_electric_fault(checked)
        self.electric_fault_changed.emit(checked)
    
    def _on_hydraulic_toggled(self, checked: bool):
        """Handler do checkbox de falha hidráulica."""
        self.fault_injector.set_hydraulic_fault(checked)
        self.hydraulic_fault_changed.emit(checked)
    
    def _on_warning_toggled(self, checked: bool):
        """Handler do checkbox de temperatura de alerta."""
        self.slider_warning.setEnabled(checked)
        temp = self.slider_warning.value()
        self.fault_injector.set_temperature_warning(checked, temp)
        self.temperature_warning_changed.emit(checked, temp)
        
        # Desmarca overheat se warning for ativado
        if checked:
            self.chk_temp_overheat.setChecked(False)
    
    def _on_overheat_toggled(self, checked: bool):
        """Handler do checkbox de superaquecimento."""
        self.slider_overheat.setEnabled(checked)
        temp = self.slider_overheat.value()
        self.fault_injector.set_temperature_overheat(checked, temp)
        self.temperature_overheat_changed.emit(checked, temp)
        
        # Desmarca warning se overheat for ativado
        if checked:
            self.chk_temp_warning.setChecked(False)
    
    def _on_warning_slider_changed(self, value: int):
        """Handler do slider de temperatura de alerta."""
        self.lbl_warning_temp.setText(f"{value}°C")
        if self.chk_temp_warning.isChecked():
            self.fault_injector.set_temperature_warning(True, float(value))
    
    def _on_overheat_slider_changed(self, value: int):
        """Handler do slider de superaquecimento."""
        self.lbl_overheat_temp.setText(f"{value}°C")
        if self.chk_temp_overheat.isChecked():
            self.fault_injector.set_temperature_overheat(True, float(value))
    
    def _on_reset_clicked(self):
        """Handler do botão de reset."""
        # Desmarca todos os checkboxes
        self.chk_electric.setChecked(False)
        self.chk_hydraulic.setChecked(False)
        self.chk_temp_warning.setChecked(False)
        self.chk_temp_overheat.setChecked(False)
        
        # Reset no backend
        self.fault_injector.reset_all_faults()
        self.reset_all_requested.emit()

