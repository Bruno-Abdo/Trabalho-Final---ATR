"""
app.py

Ponto de entrada da aplicação de Simulação da Mina.

Responsabilidades:
- Criar instâncias de Simulator, Publisher e MainWindow
- Conectar callbacks (Simulator → UI e Simulator → MQTT)
- Configurar QTimer para chamar simulator.step() periodicamente
- Iniciar o QApplication
"""

from __future__ import annotations
import sys
import logging

from PyQt6.QtWidgets import QApplication
from PyQt6.QtCore import QTimer

from model.truck_state import TruckState
from core.simulator import Simulator, SimulatorConfig
from core.publisher import MqttPublisher
from ui.main_window import MainWindow


# Configuração de logging
logging.basicConfig(
    level=logging.INFO,
    format='[%(levelname)s] %(name)s: %(message)s'
)
logger = logging.getLogger(__name__)


# Configurações padrão
MQTT_HOST = "localhost"
MQTT_PORT = 1883
TRUCK_ID = "001"
SIMULATION_PERIOD_MS = 50  # 50 ms (20 Hz)


def main() -> int:
    """
    Função principal da aplicação.
    
    Returns:
        Código de saída da aplicação.
    """
    logger.info("Iniciando Simulação da Mina...")
    
    # 1. Criar aplicação PyQt6
    app = QApplication(sys.argv)
    app.setApplicationName("Simulação da Mina - ATR")
    
    # 2. Criar componentes principais
    
    # Estado inicial do caminhão
    initial_state = TruckState(
        i_posicao_x=0.0,
        i_posicao_y=0.0,
        i_angulo_x=0.0,
        i_temperatura=25.0,
    )
    
    # Configuração do simulador
    sim_config = SimulatorConfig(
        dt_default=SIMULATION_PERIOD_MS / 1000.0,  # Converte ms para segundos
        ambient_temp=25.0,
    )
    
    # Simulador
    simulator = Simulator(
        state=initial_state,
        config=sim_config,
    )
    logger.info("Simulador criado.")
    
    # Publisher MQTT
    publisher = MqttPublisher(
        host=MQTT_HOST,
        port=MQTT_PORT,
        base_topic="atr/truck",
    )
    
    try:
        publisher.connect()
        logger.info(f"Publisher MQTT conectado em {MQTT_HOST}:{MQTT_PORT}")
    except Exception as e:
        logger.warning(f"Falha ao conectar MQTT: {e}. Continuando sem MQTT.")
    
    # Registrar callback do publisher no simulador
    simulator.register_callback(publisher.make_sim_callback(TRUCK_ID))
    
    # 3. Criar janela principal
    main_window = MainWindow(simulator)
    
    # Registrar callback da UI no simulador
    simulator.register_callback(
        lambda state, sensors: main_window.update_display(state, sensors)
    )
    
    # 4. Configurar timer para chamar simulator.step() periodicamente
    timer = QTimer()
    timer.timeout.connect(simulator.step)
    timer.start(SIMULATION_PERIOD_MS)
    logger.info(f"Timer de simulação iniciado (período: {SIMULATION_PERIOD_MS} ms)")
    
    # 5. Exibir janela e executar aplicação
    main_window.show()
    logger.info("Janela principal exibida. Aplicação pronta.")
    
    # 6. Loop de eventos Qt
    exit_code = app.exec()
    
    # 7. Cleanup ao sair
    simulator.stop()
    publisher.disconnect()
    logger.info("Aplicação encerrada.")
    
    return exit_code


if __name__ == "__main__":
    sys.exit(main())

