"""
ui/main_window.py

Janela principal da Simulação da Mina com interface visual aprimorada.
Utiliza ícones nativos do Qt (QStyle) para melhor apresentação.
"""

from __future__ import annotations

import logging
import os
import signal
import subprocess
import sys
from pathlib import Path
from typing import TYPE_CHECKING, Any, Dict, Optional

from PyQt6.QtWidgets import (
    QMainWindow,
    QWidget,
    QVBoxLayout,
    QTabWidget,
    QLabel,
    QPushButton,
    QHBoxLayout,
    QStatusBar,
    QMessageBox,
    QFrame,
)
from PyQt6.QtCore import Qt, pyqtSignal, QTimer, QSize
from PyQt6.QtGui import QIcon, QPixmap, QPalette, QColor

if TYPE_CHECKING:
    from core.simulator import Simulator

logger = logging.getLogger(__name__)


class MainWindow(QMainWindow):
    """
    Janela principal da aplicação de Simulação da Mina.
    
    Responsabilidades:
    - Organizar abas (dashboard, config, faults)
    - Controles básicos: Start/Stop simulação
    - Gerenciar processo externo do sistema embarcado (Etapa 1)
    - Barra de status com informações da simulação
    """

    # Sinais para comunicação entre UI e simulador
    start_simulation = pyqtSignal()
    stop_simulation = pyqtSignal()

    # Caminho relativo para o executável da Etapa 1
    EMBEDDED_EXECUTABLE_PATH = "../build/project_output/atr"

    def __init__(self, simulator: Simulator, parent: QWidget | None = None) -> None:
        """
        Inicializa a janela principal.

        Args:
            simulator: Instância do Simulator para controle da simulação.
            parent: Widget pai (opcional).
        """
        super().__init__(parent)
        self.simulator = simulator

        # Cache de ícones do sistema
        self._icons = self._load_system_icons()

        # Gerenciamento do processo externo (sistema embarcado)
        self._embedded_process: Optional[subprocess.Popen] = None
        self._process_monitor_timer = QTimer(self)
        self._process_monitor_timer.timeout.connect(self._check_process_health)
        self._process_monitor_timer.setInterval(1000)  # Verifica a cada 1s

        self._setup_ui()
        self._connect_signals()

    def _load_system_icons(self) -> Dict[str, QIcon]:
        """
        Carrega ícones nativos do sistema Qt.
        Estes ícones são multiplataforma e não requerem arquivos externos.
        """
        style = self.style()
        return {
            'play': style.standardIcon(style.StandardPixmap.SP_MediaPlay),
            'pause': style.standardIcon(style.StandardPixmap.SP_MediaPause),
            'stop': style.standardIcon(style.StandardPixmap.SP_MediaStop),
            'settings': style.standardIcon(style.StandardPixmap.SP_FileDialogDetailedView),
            'warning': style.standardIcon(style.StandardPixmap.SP_MessageBoxWarning),
            'computer': style.standardIcon(style.StandardPixmap.SP_ComputerIcon),
            'chart': style.standardIcon(style.StandardPixmap.SP_FileDialogContentsView),
            'info': style.standardIcon(style.StandardPixmap.SP_MessageBoxInformation),
            'refresh': style.standardIcon(style.StandardPixmap.SP_BrowserReload),
        }

    def _setup_ui(self) -> None:
        """Configura a interface gráfica."""
        self.setWindowTitle("Simulação da Mina - ATR 2025/2")
        self.setMinimumSize(1000, 700)
        
        # Define ícone da janela
        self.setWindowIcon(self._icons['computer'])

        # Widget central
        central_widget = QWidget()
        self.setCentralWidget(central_widget)

        # Layout principal vertical
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(10, 10, 10, 10)
        main_layout.setSpacing(10)

        # Barra de controle superior
        control_bar = self._create_control_bar()
        main_layout.addWidget(control_bar)

        # Separador visual
        separator = QFrame()
        separator.setFrameShape(QFrame.Shape.HLine)
        separator.setFrameShadow(QFrame.Shadow.Sunken)
        main_layout.addWidget(separator)

        # Abas (Dashboard, Config, Falhas)
        self.tab_widget = QTabWidget()
        self.tab_widget.setDocumentMode(True)  # Visual mais moderno
        main_layout.addWidget(self.tab_widget)

        # Placeholder para abas futuras
        self._add_placeholder_tabs()

        # Aplica stylesheet global
        self._apply_stylesheet()

    def _create_control_bar(self) -> QWidget:
        """Cria barra de controle com botões Start/Stop."""
        control_widget = QWidget()
        control_layout = QHBoxLayout(control_widget)
        control_layout.setSpacing(15)

        # Label informativo
        label = QLabel("Controle da Simulação:")
        label.setStyleSheet("font-weight: bold; font-size: 11pt;")
        control_layout.addWidget(label)

        # Botão Start com ícone
        self.btn_start = QPushButton(self._icons['play'], " Iniciar")
        self.btn_start.setMinimumWidth(120)
        self.btn_start.setMinimumHeight(40)
        self.btn_start.setIconSize(QSize(20, 20))
        self.btn_start.setCursor(Qt.CursorShape.PointingHandCursor)
        self.btn_start.setStyleSheet("""
            QPushButton {
                background-color: #28a745;
                color: white;
                font-weight: bold;
                font-size: 10pt;
                padding: 8px 16px;
                border: none;
                border-radius: 6px;
            }
            QPushButton:hover {
                background-color: #218838;
            }
            QPushButton:pressed {
                background-color: #1e7e34;
            }
            QPushButton:disabled {
                background-color: #6c757d;
                color: #adb5bd;
            }
        """)
        self.btn_start.clicked.connect(self._on_start_clicked)
        control_layout.addWidget(self.btn_start)

        # Botão Stop com ícone
        self.btn_stop = QPushButton(self._icons['stop'], " Parar")
        self.btn_stop.setMinimumWidth(120)
        self.btn_stop.setMinimumHeight(40)
        self.btn_stop.setIconSize(QSize(20, 20))
        self.btn_stop.setEnabled(False)
        self.btn_stop.setCursor(Qt.CursorShape.PointingHandCursor)
        self.btn_stop.setStyleSheet("""
            QPushButton {
                background-color: #dc3545;
                color: white;
                font-weight: bold;
                font-size: 10pt;
                padding: 8px 16px;
                border: none;
                border-radius: 6px;
            }
            QPushButton:hover {
                background-color: #c82333;
            }
            QPushButton:pressed {
                background-color: #bd2130;
            }
            QPushButton:disabled {
                background-color: #6c757d;
                color: #adb5bd;
            }
        """)
        self.btn_stop.clicked.connect(self._on_stop_clicked)
        control_layout.addWidget(self.btn_stop)

        # Separador vertical
        v_separator = QFrame()
        v_separator.setFrameShape(QFrame.Shape.VLine)
        v_separator.setFrameShadow(QFrame.Shadow.Sunken)
        control_layout.addWidget(v_separator)

        # Frame de status do processo
        process_frame = QFrame()
        process_frame.setFrameShape(QFrame.Shape.StyledPanel)
        process_frame.setStyleSheet("""
            QFrame {
                background-color: #f8f9fa;
                border: 1px solid #dee2e6;
                border-radius: 6px;
                padding: 5px;
            }
        """)
        process_layout = QHBoxLayout(process_frame)
        process_layout.setContentsMargins(10, 5, 10, 5)

        # Ícone de status
        self.lbl_process_icon = QLabel()
        self.lbl_process_icon.setPixmap(
            self._create_status_pixmap(QColor(220, 53, 69))  # Vermelho
        )
        process_layout.addWidget(self.lbl_process_icon)

        # Texto de status
        self.lbl_process_status = QLabel("Sistema Embarcado: Parado")
        self.lbl_process_status.setStyleSheet("font-size: 10pt; font-weight: 500;")
        process_layout.addWidget(self.lbl_process_status)

        control_layout.addWidget(process_frame)
        control_layout.addStretch()

        return control_widget

    def _create_status_pixmap(self, color: QColor, size: int = 12) -> QPixmap:
        """
        Cria um pixmap circular colorido para indicador de status.
        
        Args:
            color: Cor do indicador
            size: Tamanho em pixels
        """
        pixmap = QPixmap(size, size)
        pixmap.fill(Qt.GlobalColor.transparent)
        
        from PyQt6.QtGui import QPainter, QBrush
        painter = QPainter(pixmap)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setBrush(QBrush(color))
        painter.setPen(Qt.PenStyle.NoPen)
        painter.drawEllipse(0, 0, size, size)
        painter.end()
        
        return pixmap

    def _add_placeholder_tabs(self) -> None:
        """Adiciona abas placeholder que serão substituídas posteriormente."""
        
        # Aba 1: Dashboard
        from ui.dashboard import DashboardWidget
        self.dashboard_widget = DashboardWidget()
        self.tab_widget.addTab(
            self.dashboard_widget,
            self._icons['chart'],
            " Dashboard"
        )

        # Aba 2: Falhas FUNCIONAL (NOVO)
        from ui.faults_panel import FaultsPanel
        self.faults_panel = FaultsPanel(self.simulator.fault_injector)
        self.tab_widget.addTab(
            self.faults_panel,
            self._icons['warning'],
            " Falhas"
        )

    def _create_placeholder_widget(self, title: str, items: list) -> QWidget:
        """
        Cria widget placeholder estilizado para as abas.
        
        Args:
            title: Título da seção
            items: Lista de itens a exibir
        """
        widget = QWidget()
        layout = QVBoxLayout(widget)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.setSpacing(20)

        # Título
        title_label = QLabel(title)
        title_label.setStyleSheet("""
            font-size: 18pt;
            font-weight: bold;
            color: #495057;
        """)
        title_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(title_label)

        # Descrição
        desc_label = QLabel("Aqui será possível:")
        desc_label.setStyleSheet("font-size: 11pt; color: #6c757d;")
        desc_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(desc_label)

        # Lista de funcionalidades
        items_text = "\n".join(items)
        items_label = QLabel(items_text)
        items_label.setStyleSheet("""
            font-size: 11pt;
            color: #495057;
            line-height: 1.6;
        """)
        items_label.setAlignment(Qt.AlignmentFlag.AlignLeft)
        items_label.setWordWrap(True)
        layout.addWidget(items_label)

        layout.addStretch()
        return widget

    def _apply_stylesheet(self) -> None:
        """Aplica stylesheet global para melhor aparência."""
        self.setStyleSheet("""
            QMainWindow {
                background-color: #ffffff;
            }
            QTabWidget::pane {
                border: 1px solid #dee2e6;
                border-radius: 4px;
                background-color: white;
            }
            QTabBar::tab {
                background-color: #e9ecef;
                color: #495057;
                padding: 10px 20px;
                margin-right: 2px;
                border-top-left-radius: 4px;
                border-top-right-radius: 4px;
                font-weight: 500;
            }
            QTabBar::tab:selected {
                background-color: white;
                border-bottom: 2px solid #007bff;
            }
            QTabBar::tab:hover {
                background-color: #dee2e6;
            }
            QStatusBar {
                background-color: #f8f9fa;
                border-top: 1px solid #dee2e6;
                font-size: 10pt;
            }
        """)

    def _connect_signals(self) -> None:
        """Conecta sinais internos."""
        self.start_simulation.connect(self._start_simulator)
        self.stop_simulation.connect(self._stop_simulator)

    # ========================================================================
    # Gerenciamento do Processo Externo (Sistema Embarcado)
    # ========================================================================

    def _start_embedded_process(self) -> bool:
        """
        Inicia o processo do sistema embarcado (executável da Etapa 1).

        Returns:
            True se o processo foi iniciado com sucesso, False caso contrário.
        """
        # Resolve caminho absoluto do executável
        exec_path = Path(self.EMBEDDED_EXECUTABLE_PATH).resolve()

        if not exec_path.exists():
            logger.error("Executável não encontrado: %s", exec_path)
            QMessageBox.critical(
                self,
                "Erro ao Iniciar",
                f"Executável do sistema embarcado não encontrado:\n\n{exec_path}\n\n"
                "Certifique-se de que a Etapa 1 foi compilada corretamente.",
                QMessageBox.StandardButton.Ok
            )
            return False

        try:
            logger.info("Iniciando processo externo: %s", exec_path)
            
            # Inicia o processo em um novo grupo (importante para Unix)
            if sys.platform == "win32":
                creation_flags = subprocess.CREATE_NEW_PROCESS_GROUP
            else:
                creation_flags = 0

            self._embedded_process = subprocess.Popen(
                [str(exec_path)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                cwd=exec_path.parent,
                creationflags=creation_flags if sys.platform == "win32" else 0,
                preexec_fn=os.setsid if sys.platform != "win32" else None,
            )

            logger.info("Processo iniciado com PID: %d", self._embedded_process.pid)
            self._update_process_status(running=True)
            self._process_monitor_timer.start()
            return True

        except FileNotFoundError:
            logger.error("Falha ao executar: arquivo não encontrado")
            QMessageBox.critical(
                self, "Erro", "Não foi possível executar o arquivo.",
                QMessageBox.StandardButton.Ok
            )
            return False
        except PermissionError:
            logger.error("Falha ao executar: sem permissão")
            QMessageBox.critical(
                self,
                "Erro",
                f"Sem permissão para executar o arquivo.\n\n"
                f"No Linux/macOS, execute:\nchmod +x {exec_path}",
                QMessageBox.StandardButton.Ok
            )
            return False
        except Exception as exc:
            logger.exception("Erro inesperado ao iniciar processo")
            QMessageBox.critical(
                self, "Erro", f"Erro ao iniciar processo:\n\n{exc}",
                QMessageBox.StandardButton.Ok
            )
            return False

    def _stop_embedded_process(self) -> None:
        """
        Para o processo do sistema embarcado de forma graciosa.
        """
        if self._embedded_process is None:
            return

        pid = self._embedded_process.pid
        logger.info("Parando processo PID %d...", pid)

        try:
            # Tenta terminar graciosamente
            if sys.platform == "win32":
                self._embedded_process.terminate()
            else:
                os.killpg(os.getpgid(pid), signal.SIGTERM)

            # Aguarda até 3 segundos
            try:
                self._embedded_process.wait(timeout=3.0)
                logger.info("Processo terminado graciosamente")
            except subprocess.TimeoutExpired:
                logger.warning("Processo não respondeu, forçando terminação...")
                if sys.platform == "win32":
                    self._embedded_process.kill()
                else:
                    os.killpg(os.getpgid(pid), signal.SIGKILL)
                self._embedded_process.wait()
                logger.info("Processo forçadamente terminado")

        except ProcessLookupError:
            logger.info("Processo já estava terminado")
        except Exception as exc:
            logger.exception("Erro ao parar processo")
            QMessageBox.warning(
                self, "Aviso", f"Erro ao parar processo:\n\n{exc}",
                QMessageBox.StandardButton.Ok
            )
        finally:
            self._embedded_process = None
            self._update_process_status(running=False)
            self._process_monitor_timer.stop()

    def _check_process_health(self) -> None:
        """
        Verifica se o processo ainda está rodando.
        """
        if self._embedded_process is None:
            return

        poll_result = self._embedded_process.poll()
        if poll_result is not None:
            logger.warning(
                "Processo externo terminou inesperadamente (código: %d)",
                poll_result,
            )
            self._embedded_process = None
            self._update_process_status(running=False)
            self._process_monitor_timer.stop()

            if self.simulator.is_running:
                self.stop_simulation.emit()

            QMessageBox.warning(
                self,
                "Processo Terminado",
                f"O sistema embarcado terminou inesperadamente.\n\n"
                f"Código de saída: {poll_result}",
                QMessageBox.StandardButton.Ok
            )

    def _update_process_status(self, running: bool) -> None:
        """Atualiza indicador visual do status do processo."""
        if running:
            self.lbl_process_status.setText("Sistema Embarcado: Rodando")
            self.lbl_process_status.setStyleSheet(
                "font-size: 10pt; font-weight: 500; color: #28a745;"
            )
            self.lbl_process_icon.setPixmap(
                self._create_status_pixmap(QColor(40, 167, 69))  # Verde
            )
        else:
            self.lbl_process_status.setText("Sistema Embarcado: Parado")
            self.lbl_process_status.setStyleSheet(
                "font-size: 10pt; font-weight: 500; color: #dc3545;"
            )
            self.lbl_process_icon.setPixmap(
                self._create_status_pixmap(QColor(220, 53, 69))  # Vermelho
            )

    # ========================================================================
    # Handlers de Botões
    # ========================================================================

    def _on_start_clicked(self) -> None:
        """Handler do botão Iniciar."""
        if not self._start_embedded_process():
            return
        self.start_simulation.emit()

    def _on_stop_clicked(self) -> None:
        """Handler do botão Parar."""
        self.stop_simulation.emit()
        self._stop_embedded_process()

    def _start_simulator(self) -> None:
        """Inicia o simulador e atualiza UI."""
        self.simulator.start()
        self.btn_start.setEnabled(False)
        self.btn_stop.setEnabled(True)

    def _stop_simulator(self) -> None:
        """Para o simulador e atualiza UI."""
        self.simulator.stop()
        self.btn_start.setEnabled(True)
        self.btn_stop.setEnabled(False)
        self._update_status_idle()

    def _update_status_idle(self) -> None:
        """Atualiza status bar para estado ocioso."""

    # ========================================================================
    # Atualização de Display
    # ========================================================================

    def update_display(self, state: Any, sensors: Dict[str, Any]) -> None:
        """Callback chamado a cada passo de simulação."""
            # Atualiza dashboard
        self.dashboard_widget.update_data(state, sensors)
        # Atualiza status bar (código existente)
        sim_time = self.simulator.sim_time
        # self.status_time_label.setText(f"Tempo: {sim_time:.2f}s")

        pos_x = sensors.get("i_posicao_x", 0)
        pos_y = sensors.get("i_posicao_y", 0)
        # self.status_pos_label.setText(f"Posição: ({pos_x:.1f}, {pos_y:.1f})")

        temp = sensors.get("i_temperatura", 0)
        # self._update_temperature_display(temp)

    # ========================================================================
    # Limpeza ao Fechar
    # ========================================================================

    def closeEvent(self, event) -> None:
        """
        Sobrescreve evento de fechamento para garantir limpeza de recursos.
        """
        if self._embedded_process is not None:
            reply = QMessageBox.question(
                self,
                "Confirmar Saída",
                "O sistema embarcado ainda está rodando.\n\n"
                "Deseja parar e sair?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.Yes,
            )

            if reply == QMessageBox.StandardButton.Yes:
                self._stop_embedded_process()
                event.accept()
            else:
                event.ignore()
        else:
            event.accept()

