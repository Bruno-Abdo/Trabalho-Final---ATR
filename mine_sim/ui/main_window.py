"""
Janela principal da Simulação da Mina.

Interface gráfica com navegação por abas:
    - Dashboard: Visualização em tempo real dos sensores
    - Configuração: Ajustes de variáveis e período de atualização
    - Falhas: Injeção de defeitos no caminhão
"""

from __future__ import annotations

from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QTabWidget, QGroupBox, QLabel, QPushButton,
    QCheckBox, QSpinBox, QDoubleSpinBox, QGridLayout,
    QStatusBar
)
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QFont

from core import Simulator, MqttPublisher
from model import TruckState


class MainWindow(QMainWindow):
    """
    Janela principal da aplicação de Simulação da Mina.
    
    Integra:
        - Simulator: núcleo da simulação
        - MqttPublisher: publicação MQTT
        - UI: painéis de controle e visualização
    """
    
    def __init__(
        self,
        simulator: Simulator,
        publisher: MqttPublisher | None = None,
        truck_id: str = "001"
    ):
        """
        Inicializa a janela principal.
        
        Args:
            simulator: Instância do Simulator
            publisher: Instância do MqttPublisher (opcional)
            truck_id: Identificador do caminhão
        """
        super().__init__()
        
        self.simulator = simulator
        self.publisher = publisher
        self.truck_id = truck_id
        
        # Estado da simulação
        self._is_running = False
        self._mqtt_connected = publisher.is_connected if publisher else False
        
        # Configuração da janela
        self.setWindowTitle(f"Simulação da Mina - Caminhão {truck_id}")
        self.setMinimumSize(800, 600)
        
        # Conecta callback do simulador
        self.simulator.register_callback(self._on_state_updated)
        
        # Cria interface
        self._setup_ui()
        
        # Timer para atualizar status MQTT
        self._status_timer = QTimer(self)
        self._status_timer.timeout.connect(self._update_status_bar)
        self._status_timer.start(1000)  # Atualiza a cada 1 segundo
    
    # =======================================================================
    # CONFIGURAÇÃO DA INTERFACE
    # =======================================================================
    
    def _setup_ui(self):
        """Configura a interface gráfica."""
        # Widget central
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        # Layout principal
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(10, 10, 10, 10)
        
        # Cabeçalho
        header = self._create_header()
        main_layout.addWidget(header)
        
        # Abas de navegação
        self.tabs = QTabWidget()
        self.tabs.setDocumentMode(True)
        
        # Aba 1: Dashboard
        self.dashboard_tab = self._create_dashboard_tab()
        self.tabs.addTab(self.dashboard_tab, "Dashboard")
        
        # Aba 2: Configuração
        self.config_tab = self._create_config_tab()
        self.tabs.addTab(self.config_tab, "Configuração")
        
        # Aba 3: Falhas
        self.faults_tab = self._create_faults_tab()
        self.tabs.addTab(self.faults_tab, "Falhas")
        
        main_layout.addWidget(self.tabs)
        
        # Barra de status
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self._update_status_bar()
    
    def _create_header(self) -> QWidget:
        """Cria o cabeçalho com botões de controle."""
        header = QWidget()
        layout = QHBoxLayout(header)
        
        # Título
        title = QLabel(f"Caminhão {self.truck_id}")
        title_font = QFont()
        title_font.setPointSize(16)
        title_font.setBold(True)
        title.setFont(title_font)
        layout.addWidget(title)
        
        layout.addStretch()
        
        # Botão Iniciar/Parar
        self.btn_start_stop = QPushButton("Iniciar Simulação")
        self.btn_start_stop.setMinimumWidth(150)
        self.btn_start_stop.clicked.connect(self._toggle_simulation)
        layout.addWidget(self.btn_start_stop)
        
        # Botão Reset
        btn_reset = QPushButton("Reset")
        btn_reset.clicked.connect(self._reset_simulation)
        layout.addWidget(btn_reset)
        
        return header
    
    # =======================================================================
    # ABA: DASHBOARD
    # =======================================================================
    
    def _create_dashboard_tab(self) -> QWidget:
        """Cria aba de visualização em tempo real."""
        tab = QWidget()
        layout = QVBoxLayout(tab)
        layout.setSpacing(15)
        
        # Card: Posição
        position_group = QGroupBox("Posição e Orientação")
        position_layout = QGridLayout(position_group)
        
        position_layout.addWidget(QLabel("Posição X:"), 0, 0)
        self.lbl_pos_x = QLabel("0 m")
        self.lbl_pos_x.setStyleSheet("font-size: 14pt; font-weight: bold;")
        position_layout.addWidget(self.lbl_pos_x, 0, 1)
        
        position_layout.addWidget(QLabel("Posição Y:"), 1, 0)
        self.lbl_pos_y = QLabel("0 m")
        self.lbl_pos_y.setStyleSheet("font-size: 14pt; font-weight: bold;")
        position_layout.addWidget(self.lbl_pos_y, 1, 1)
        
        position_layout.addWidget(QLabel("Ângulo:"), 2, 0)
        self.lbl_angle = QLabel("0°")
        self.lbl_angle.setStyleSheet("font-size: 14pt; font-weight: bold;")
        position_layout.addWidget(self.lbl_angle, 2, 1)
        
        position_layout.addWidget(QLabel("Velocidade:"), 3, 0)
        self.lbl_velocity = QLabel("0.0 m/s")
        self.lbl_velocity.setStyleSheet("font-size: 14pt; font-weight: bold;")
        position_layout.addWidget(self.lbl_velocity, 3, 1)
        
        layout.addWidget(position_group)
        
        # Card: Temperatura
        temp_group = QGroupBox("Temperatura")
        temp_layout = QVBoxLayout(temp_group)
        
        self.lbl_temperature = QLabel("25°C")
        self.lbl_temperature.setStyleSheet("font-size: 24pt; font-weight: bold;")
        self.lbl_temperature.setAlignment(Qt.AlignmentFlag.AlignCenter)
        temp_layout.addWidget(self.lbl_temperature)
        
        self.lbl_temp_status = QLabel("NORMAL")
        self.lbl_temp_status.setStyleSheet("font-size: 12pt; color: green;")
        self.lbl_temp_status.setAlignment(Qt.AlignmentFlag.AlignCenter)
        temp_layout.addWidget(self.lbl_temp_status)
        
        layout.addWidget(temp_group)
        
        # Card: Falhas
        faults_group = QGroupBox("Status de Falhas")
        faults_layout = QGridLayout(faults_group)
        
        faults_layout.addWidget(QLabel("Falha Elétrica:"), 0, 0)
        self.lbl_fault_electrical = QLabel("OK")
        self.lbl_fault_electrical.setStyleSheet("color: green; font-weight: bold;")
        faults_layout.addWidget(self.lbl_fault_electrical, 0, 1)
        
        faults_layout.addWidget(QLabel("Falha Hidráulica:"), 1, 0)
        self.lbl_fault_hydraulic = QLabel("OK")
        self.lbl_fault_hydraulic.setStyleSheet("color: green; font-weight: bold;")
        faults_layout.addWidget(self.lbl_fault_hydraulic, 1, 1)
        
        layout.addWidget(faults_group)
        
        # Card: Atuadores
        actuators_group = QGroupBox("Atuadores")
        actuators_layout = QGridLayout(actuators_group)
        
        actuators_layout.addWidget(QLabel("Aceleração:"), 0, 0)
        self.lbl_acceleration = QLabel("0%")
        actuators_layout.addWidget(self.lbl_acceleration, 0, 1)
        
        actuators_layout.addWidget(QLabel("Direção:"), 1, 0)
        self.lbl_steering = QLabel("0°")
        actuators_layout.addWidget(self.lbl_steering, 1, 1)
        
        layout.addWidget(actuators_group)
        
        layout.addStretch()
        
        return tab
    
    # =======================================================================
    # ABA: CONFIGURAÇÃO
    # =======================================================================
    
    def _create_config_tab(self) -> QWidget:
        """Cria aba de configuração."""
        tab = QWidget()
        layout = QVBoxLayout(tab)
        
        # Período de atualização
        period_group = QGroupBox("Período de Atualização")
        period_layout = QHBoxLayout(period_group)
        
        period_layout.addWidget(QLabel("Intervalo (ms):"))
        
        self.spin_period = QSpinBox()
        self.spin_period.setRange(10, 10000)
        self.spin_period.setValue(1000)
        self.spin_period.setSuffix(" ms")
        period_layout.addWidget(self.spin_period)
        
        btn_apply_period = QPushButton("Aplicar")
        btn_apply_period.clicked.connect(self._apply_period)
        period_layout.addWidget(btn_apply_period)
        
        period_layout.addStretch()
        
        layout.addWidget(period_group)
        
        # Controle de atuadores
        actuators_group = QGroupBox("Controle Manual de Atuadores")
        actuators_layout = QGridLayout(actuators_group)
        
        actuators_layout.addWidget(QLabel("Aceleração (%):"), 0, 0)
        self.spin_acceleration = QDoubleSpinBox()
        self.spin_acceleration.setRange(-100.0, 100.0)
        self.spin_acceleration.setValue(0.0)
        self.spin_acceleration.setSuffix(" %")
        actuators_layout.addWidget(self.spin_acceleration, 0, 1)
        
        actuators_layout.addWidget(QLabel("Direção (°):"), 1, 0)
        self.spin_steering = QDoubleSpinBox()
        self.spin_steering.setRange(-180.0, 180.0)
        self.spin_steering.setValue(0.0)
        self.spin_steering.setSuffix(" °")
        actuators_layout.addWidget(self.spin_steering, 1, 1)
        
        btn_apply_actuators = QPushButton("Aplicar Comandos")
        btn_apply_actuators.clicked.connect(self._apply_actuators)
        actuators_layout.addWidget(btn_apply_actuators, 2, 0, 1, 2)
        
        layout.addWidget(actuators_group)
        
        layout.addStretch()
        
        return tab
    
    # =======================================================================
    # ABA: FALHAS
    # =======================================================================
    
    def _create_faults_tab(self) -> QWidget:
        """Cria aba de injeção de falhas."""
        tab = QWidget()
        layout = QVBoxLayout(tab)
        
        info_label = QLabel(
            "⚠️ Use os controles abaixo para injetar falhas no caminhão.\n"
            "As falhas serão refletidas nos sensores e publicadas via MQTT."
        )
        info_label.setWordWrap(True)
        layout.addWidget(info_label)
        
        # Falhas disponíveis
        faults_group = QGroupBox("Injeção de Falhas")
        faults_layout = QVBoxLayout(faults_group)
        
        # Falha elétrica
        self.chk_fault_electrical = QCheckBox("Falha Elétrica")
        self.chk_fault_electrical.stateChanged.connect(self._apply_faults)
        faults_layout.addWidget(self.chk_fault_electrical)
        
        # Falha hidráulica
        self.chk_fault_hydraulic = QCheckBox("Falha Hidráulica")
        self.chk_fault_hydraulic.stateChanged.connect(self._apply_faults)
        faults_layout.addWidget(self.chk_fault_hydraulic)
        
        # Botão superaquecimento
        btn_overheat = QPushButton("Forçar Superaquecimento")
        btn_overheat.clicked.connect(self._force_overheat)
        faults_layout.addWidget(btn_overheat)
        
        # Botão limpar falhas
        btn_clear = QPushButton("Limpar Todas as Falhas")
        btn_clear.clicked.connect(self._clear_all_faults)
        faults_layout.addWidget(btn_clear)
        
        layout.addWidget(faults_group)
        
        layout.addStretch()
        
        return tab
    
    # =======================================================================
    # CALLBACKS E SLOTS
    # =======================================================================
    
    def _on_state_updated(self, state: TruckState, sensors: dict):
        """
        Callback chamado quando o simulador atualiza o estado.
        
        Args:
            state: Estado físico (limpo)
            sensors: Sensores com ruído aplicado
        """
        # Atualiza dashboard com valores dos sensores (com ruído)
        self.lbl_pos_x.setText(f"{sensors['i_posicao_x']} m")
        self.lbl_pos_y.setText(f"{sensors['i_posicao_y']} m")
        self.lbl_angle.setText(f"{sensors['i_angulo_x']}°")
        self.lbl_velocity.setText(f"{state.velocidade:.2f} m/s")
        
        # Temperatura com cor baseada em thresholds
        temp = sensors['i_temperatura']
        self.lbl_temperature.setText(f"{temp}°C")
        
        if temp > 120:
            self.lbl_temp_status.setText("DEFEITO!")
            self.lbl_temp_status.setStyleSheet("font-size: 12pt; color: red; font-weight: bold;")
        elif temp > 95:
            self.lbl_temp_status.setText("ALERTA")
            self.lbl_temp_status.setStyleSheet("font-size: 12pt; color: orange; font-weight: bold;")
        else:
            self.lbl_temp_status.setText("NORMAL")
            self.lbl_temp_status.setStyleSheet("font-size: 12pt; color: green;")
        
        # Falhas
        self.lbl_fault_electrical.setText("FALHA!" if sensors['i_falha_eletrica'] else "OK")
        self.lbl_fault_electrical.setStyleSheet(
            "color: red; font-weight: bold;" if sensors['i_falha_eletrica']
            else "color: green; font-weight: bold;"
        )
        
        self.lbl_fault_hydraulic.setText("FALHA!" if sensors['i_falha_hidraulica'] else "OK")
        self.lbl_fault_hydraulic.setStyleSheet(
            "color: red; font-weight: bold;" if sensors['i_falha_hidraulica']
            else "color: green; font-weight: bold;"
        )
        
        # Atuadores
        self.lbl_acceleration.setText(f"{state.o_aceleracao:.1f}%")
        self.lbl_steering.setText(f"{state.o_direcao:.1f}°")
    
    def _toggle_simulation(self):
        """Inicia ou para a simulação."""
        if self._is_running:
            self.simulator.stop()
            self.btn_start_stop.setText("Iniciar Simulação")
            self._is_running = False
        else:
            self.simulator.start()
            self.btn_start_stop.setText("Parar Simulação")
            self._is_running = True
    
    def _reset_simulation(self):
        """Reseta o estado da simulação."""
        from model import TruckState
        self.simulator.state = TruckState()
        self.simulator.sim_time = 0.0
        
        # Limpa checkboxes de falhas
        self.chk_fault_electrical.setChecked(False)
        self.chk_fault_hydraulic.setChecked(False)
    
    def _apply_period(self):
        """Aplica novo período de atualização."""
        period_ms = self.spin_period.value()
        self.simulator.set_update_period(period_ms / 1000.0)
        self.status_bar.showMessage(f"Período atualizado para {period_ms} ms", 3000)
    
    def _apply_actuators(self):
        """Aplica comandos de atuadores."""
        accel = self.spin_acceleration.value()
        steer = self.spin_steering.value()
        self.simulator.set_actuators(aceleracao=accel, direcao=steer)
        self.status_bar.showMessage(f"Atuadores: Acel={accel}%, Dir={steer}°", 3000)
    
    def _apply_faults(self):
        """Aplica falhas selecionadas."""
        from model import set_electrical_fault, set_hydraulic_fault
        
        set_electrical_fault(self.simulator.state, self.chk_fault_electrical.isChecked())
        set_hydraulic_fault(self.simulator.state, self.chk_fault_hydraulic.isChecked())
    
    def _force_overheat(self):
        """Força temperatura crítica."""
        from model import set_overheat_fault
        set_overheat_fault(self.simulator.state, enabled=True, critical=True)
        self.status_bar.showMessage("Superaquecimento forçado!", 3000)
    
    def _clear_all_faults(self):
        """Limpa todas as falhas."""
        from model import clear_all_faults
        clear_all_faults(self.simulator.state)
        
        self.chk_fault_electrical.setChecked(False)
        self.chk_fault_hydraulic.setChecked(False)
        
        self.status_bar.showMessage("Todas as falhas removidas", 3000)
    
    def _update_status_bar(self):
        """Atualiza barra de status."""
        sim_status = "Rodando" if self._is_running else "Parado"
        mqtt_status = "✓ MQTT" if (self.publisher and self.publisher.is_connected) else "✗ MQTT"
        sim_time = f"t={self.simulator.sim_time:.1f}s"
        
        self.status_bar.showMessage(f"{sim_status} | {mqtt_status} | {sim_time}")
