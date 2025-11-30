"""
ui/main_window.py

Janela principal da Simulação da Mina.

Estrutura modular com QTabWidget para organizar:
- Dashboard (visualização em tempo real)
- Configuração (parâmetros de simulação/ruído)
- Falhas (injeção de defeitos)

Outros módulos serão integrados posteriormente.
"""

from __future__ import annotations
from typing import TYPE_CHECKING, Any, Dict

from PyQt6.QtWidgets import (
    QMainWindow,
    QWidget,
    QVBoxLayout,
    QTabWidget,
    QLabel,
    QPushButton,
    QHBoxLayout,
    QStatusBar,
)
from PyQt6.QtCore import Qt, pyqtSignal

if TYPE_CHECKING:
    from core.simulator import Simulator


class MainWindow(QMainWindow):
    """
    Janela principal da aplicação de Simulação da Mina.
    
    Responsabilidades:
    - Organizar abas (dashboard, config, faults)
    - Controles básicos: Start/Stop simulação
    - Barra de status com informações da simulação
    """
    
    # Sinais para comunicação entre UI e simulador
    start_simulation = pyqtSignal()
    stop_simulation = pyqtSignal()
    
    def __init__(self, simulator: Simulator, parent: QWidget | None = None) -> None:
        """
        Inicializa a janela principal.
        
        Args:
            simulator: Instância do Simulator para controle da simulação.
            parent: Widget pai (opcional).
        """
        super().__init__(parent)
        self.simulator = simulator
        
        self._setup_ui()
        self._connect_signals()
        
    def _setup_ui(self) -> None:
        """Configura a interface gráfica."""
        self.setWindowTitle("Simulação da Mina - ATR 2025/2")
        self.setMinimumSize(800, 600)
        
        # Widget central
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        # Layout principal vertical
        main_layout = QVBoxLayout(central_widget)
        
        # Barra de controle superior
        control_bar = self._create_control_bar()
        main_layout.addWidget(control_bar)
        
        # Abas (Dashboard, Config, Falhas)
        self.tab_widget = QTabWidget()
        main_layout.addWidget(self.tab_widget)
        
        # Placeholder para abas futuras
        self._add_placeholder_tabs()
        
        # Barra de status
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("Simulador parado. Pressione 'Iniciar' para começar.")
        
    def _create_control_bar(self) -> QWidget:
        """Cria barra de controle com botões Start/Stop."""
        control_widget = QWidget()
        control_layout = QHBoxLayout(control_widget)
        
        # Label informativo
        label = QLabel("Controle da Simulação:")
        control_layout.addWidget(label)
        
        # Botão Start
        self.btn_start = QPushButton("Iniciar")
        self.btn_start.setMinimumWidth(100)
        self.btn_start.clicked.connect(self._on_start_clicked)
        control_layout.addWidget(self.btn_start)
        
        # Botão Stop
        self.btn_stop = QPushButton("Parar")
        self.btn_stop.setMinimumWidth(100)
        self.btn_stop.setEnabled(False)
        self.btn_stop.clicked.connect(self._on_stop_clicked)
        control_layout.addWidget(self.btn_stop)
        
        control_layout.addStretch()
        
        return control_widget
    
    def _add_placeholder_tabs(self) -> None:
        """Adiciona abas placeholder que serão substituídas posteriormente."""
        # Aba 1: Dashboard (futuramente dashboard.py)
        dashboard_placeholder = QLabel(
            "Dashboard\n\n"
            "Aqui serão exibidos em tempo real:\n"
            "- Posição (x, y, ângulo)\n"
            "- Temperatura\n"
            "- Flags de falha\n"
            "- Atuadores (aceleração, direção)"
        )
        dashboard_placeholder.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.tab_widget.addTab(dashboard_placeholder, "Dashboard")
        
        # Aba 2: Configuração (futuramente config_panel.py)
        config_placeholder = QLabel(
            "Configuração\n\n"
            "Aqui será possível configurar:\n"
            "- Faixas de valores (Min/Max)\n"
            "- Taxa de atualização (período)\n"
            "- Parâmetros de ruído"
        )
        config_placeholder.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.tab_widget.addTab(config_placeholder, "Configuração")
        
        # Aba 3: Falhas (integração com faults.py)
        faults_placeholder = QLabel(
            "Injeção de Falhas\n\n"
            "Aqui será possível injetar:\n"
            "- Falha elétrica\n"
            "- Falha hidráulica\n"
            "- Superaquecimento"
        )
        faults_placeholder.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.tab_widget.addTab(faults_placeholder, "Falhas")
    
    def _connect_signals(self) -> None:
        """Conecta sinais internos."""
        self.start_simulation.connect(self._start_simulator)
        self.stop_simulation.connect(self._stop_simulator)
    
    def _on_start_clicked(self) -> None:
        """Handler do botão Iniciar."""
        self.start_simulation.emit()
    
    def _on_stop_clicked(self) -> None:
        """Handler do botão Parar."""
        self.stop_simulation.emit()
    
    def _start_simulator(self) -> None:
        """Inicia o simulador e atualiza UI."""
        self.simulator.start()
        self.btn_start.setEnabled(False)
        self.btn_stop.setEnabled(True)
        self.status_bar.showMessage("✅ Simulador em execução...")
    
    def _stop_simulator(self) -> None:
        """Para o simulador e atualiza UI."""
        self.simulator.stop()
        self.btn_start.setEnabled(True)
        self.btn_stop.setEnabled(False)
        self.status_bar.showMessage("⏸ Simulador pausado.")
    
    def update_display(self, state: Any, sensors: Dict[str, Any]) -> None:
        """
        Callback chamado a cada passo de simulação.
        
        Atualiza informações na UI (futuramente delegado às abas).
        
        Args:
            state: TruckState atual.
            sensors: Dicionário com sensores ruidosos.
        """
        # Atualiza barra de status com info básica
        sim_time = self.simulator.sim_time
        pos_x = sensors.get('i_posicao_x', 0)
        pos_y = sensors.get('i_posicao_y', 0)
        temp = sensors.get('i_temperatura', 0)
        
        status_msg = (
            f"⏱ t={sim_time:.2f}s | "
            f"📍 Pos: ({pos_x:.1f}, {pos_y:.1f}) | "
            f"🌡 Temp: {temp:.1f}°C"
        )
        self.status_bar.showMessage(status_msg)

