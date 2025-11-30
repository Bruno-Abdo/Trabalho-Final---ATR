import sys
from PyQt6.QtWidgets import QApplication

from core import Simulator, SimulatorConfig, MqttPublisher
from ui import MainWindow


def main():
    app = QApplication(sys.argv)
    
    # Cria simulador
    sim_config = SimulatorConfig(dt_default=0.1)
    simulator = Simulator(config=sim_config)
    
    try:
        publisher = MqttPublisher(host="mosquitto", port=1883)
        publisher.connect()
    except:
        publisher = None
        print("[AVISO] MQTT não disponível - continuando sem publicação")
    
    # Cria janela principal
    window = MainWindow(simulator=simulator, publisher=publisher, truck_id="001")
    window.show()
    
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
