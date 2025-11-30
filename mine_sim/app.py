import time

from core.simulator import Simulator
from core.publisher import MqttPublisher


def main() -> None:
    # 1) Instancia núcleo de simulação e publisher MQTT
    sim = Simulator()  # usa SimulatorConfig padrão (dt_default = 0.05 s, etc.)
    pub = MqttPublisher(host="mosquitto", port=1883)

    # 2) Conecta ao broker MQTT (loop de rede em thread própria)
    pub.connect()  # start_loop=True por padrão

    # 3) Registra callback de publicação no simulador
    #    Sempre que sim.step() rodar, o callback será chamado com (state, sensors_dict),
    #    e o publisher enviará sensors_dict no tópico atr/mine/truck/001/sensors.
    sim.register_callback(pub.make_sim_callback(truck_id="001"))

    # 4) Inicia a simulação
    sim.start()

    try:
        print("Simulação da Mina rodando. Pressione Ctrl+C para encerrar.")
        while True:
            # step() sem argumento usa sim.config.dt_default como passo de tempo
            sim.step()
            time.sleep(sim.config.dt_default)
    except KeyboardInterrupt:
        print("\nEncerrando simulação...")
    finally:
        # 5) Finaliza simulação e MQTT de forma limpa
        sim.stop()
        pub.disconnect()


if __name__ == "__main__":
    main()
