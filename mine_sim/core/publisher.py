"""
core/publisher.py

Cliente MQTT Publisher para a Simulação da Mina.

Responsável por enviar, via MQTT, os dados dos sensores do caminhão
gerados pela Simulação da Mina (Etapa 2 do trabalho "2025_2 - ATR - Trabalho Final").

- Atua apenas como PUBLISHER (sem subscribers).
- Não depende de PyQt; pode ser usado tanto em app GUI quanto em scripts.
- Integra-se com o núcleo de simulação via callback (make_sim_callback).

Tópicos:
    atr/truck/<id>/sensors

Payload (JSON):
    {
        "truck_id": "<id>",
        "timestamp": <epoch_seconds>,
        "sensors": {
            "i_posicao_x": ...,
            "i_posicao_y": ...,
            "i_angulo_x": ...,
            "i_temperatura": ...,
            "i_falha_eletrica": ...,
            "i_falha_hidraulica": ...
        }
    }
"""

from __future__ import annotations

import json
import logging
import socket
import time
from typing import Any, Callable, Dict, Optional

try:
    import paho.mqtt.client as mqtt
except ImportError as exc:  # pragma: no cover
    raise ImportError(
        "O módulo 'paho-mqtt' não está instalado. "
        "Adicione 'paho-mqtt' ao requirements.txt e instale com pip."
    ) from exc


logger = logging.getLogger(__name__)


class MqttPublisher:
    """
    Cliente MQTT minimalista para publicação do estado dos sensores do caminhão.

    Interface pública:
        - connect()
        - disconnect()
        - is_connected (property)
        - publish_state(truck_id: str, sensors: Dict[str, Any])
        - make_sim_callback(truck_id: str) -> Callable[[Any, Dict[str, Any]], None]

    Uso típico:

        publisher = MqttPublisher(host="localhost", port=1883)
        publisher.connect()

        # Em app.py, integrando com o Simulator:
        sim.register_callback(publisher.make_sim_callback(truck_id="001"))
    """

    def __init__(
        self,
        host: str = "localhost",
        port: int = 1883,
        base_topic: str = "atr/truck",
        client_id: Optional[str] = None,
        keepalive: int = 60,
        qos: int = 0,
        retain: bool = False,
    ) -> None:
        """
        Inicializa o publisher, mas NÃO conecta ainda ao broker.

        Args:
            host      : endereço do broker MQTT.
            port      : porta do broker (padrão: 1883).
            base_topic: prefixo dos tópicos, ex.: "atr/truck".
            client_id : ID do cliente MQTT; se None, é gerado automaticamente.
            keepalive : keepalive em segundos.
            qos       : QoS padrão das publicações (0, 1 ou 2).
            retain    : se True, mensagens são publicadas com retain.
        """
        self.host = host
        self.port = port
        self.base_topic = base_topic.rstrip("/")
        self.keepalive = keepalive
        self.qos = qos
        self.retain = retain

        # Identificador único do cliente MQTT
        if client_id is None:
            hostname = socket.gethostname()
            client_id = f"mine-sim-pub-{hostname}-{int(time.time())}"
        self.client_id = client_id

        # Instância do cliente MQTT (paho-mqtt)
        self._client = mqtt.Client(client_id=self.client_id, clean_session=True)
        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect

        self._connected: bool = False

    @staticmethod
    def _quantize_sensors(sensors: Dict[str, Any]) -> Dict[str, Any]:
        """
        Converte o dicionário de sensores contínuos para ints/bools
        conforme Tabela 1.
        """
        out: Dict[str, Any] = {}

        for name, val in sensors.items():
            if name in ("i_falha_eletrica", "i_falha_hidraulica"):
                out[name] = bool(val)
            else:
                # float -> int (mantendo ruído, mas discretizado)
                out[name] = int(round(float(val)))
        return out

    # ---------------------------------------------------------------------
    # Callbacks internos do cliente MQTT
    # ---------------------------------------------------------------------

    def _on_connect(self, client: mqtt.Client, userdata: Any, flags: Dict[str, Any], rc: int) -> None:
        if rc == 0:
            logger.info("MQTT conectado em %s:%d (client_id=%s)", self.host, self.port, self.client_id)
            self._connected = True
        else:
            logger.error("Falha na conexão MQTT (rc=%s). Verifique broker/porta.", rc)
            self._connected = False

    def _on_disconnect(self, client: mqtt.Client, userdata: Any, rc: int) -> None:
        self._connected = False
        if rc == 0:
            logger.info("MQTT desconectado normalmente.")
        else:
            logger.warning("MQTT desconectado com erro (rc=%s).", rc)

    # ---------------------------------------------------------------------
    # API pública
    # ---------------------------------------------------------------------

    def connect(self, *, start_loop: bool = True) -> None:
        """
        Conecta ao broker MQTT.

        Se start_loop=True (padrão), inicia o loop de rede em uma thread
        interna (loop_start), evitando bloqueio da UI ou do loop principal.
        """
        logger.info("Conectando ao broker MQTT em %s:%d ...", self.host, self.port)
        self._client.connect(self.host, self.port, self.keepalive)

        if start_loop:
            self._client.loop_start()

    def disconnect(self, *, stop_loop: bool = True) -> None:
        """
        Desconecta do broker MQTT.

        Se stop_loop=True (padrão), encerra o loop de rede se ele estiver ativo.
        """
        if stop_loop:
            self._client.loop_stop()
        self._client.disconnect()

    @property
    def is_connected(self) -> bool:
        """Retorna True se o cliente está conectado ao broker."""
        return self._connected

    def publish_state(self, truck_id: str, sensors: Dict[str, Any]) -> None:
        """
        Publica o estado atual dos sensores de um caminhão.

        Args:
            truck_id: identificador lógico do caminhão (ex.: "001").
            sensors : dicionário com as variáveis de sensor já com ruído,
                      normalmente obtido via TruckState.get_sensor_dict().
        """
        if not self._connected:
            logger.debug("MQTT não conectado; publicação ignorada.")
            return

        topic = f"{self.base_topic}/{truck_id}/sensors"
        quantized_sensors: Dict[str, Any] = self._quantize_sensors(sensors)

        payload_dict: Dict[str, Any] = {
            "truck_id": truck_id,
            "timestamp": time.time(),
            "sensors": quantized_sensors,
        }

        try:
            payload = json.dumps(payload_dict)
        except (TypeError, ValueError) as exc:
            logger.error("Falha ao serializar payload MQTT: %s", exc)
            return

        result = self._client.publish(topic, payload=payload, qos=self.qos, retain=self.retain)
        if result.rc != mqtt.MQTT_ERR_SUCCESS:
            logger.error("Erro ao publicar no tópico %s (rc=%s).", topic, result.rc)
        else:
            logger.debug("Publicado em %s: %s", topic, payload)

    # ---------------------------------------------------------------------
    # Integração com o Simulator
    # ---------------------------------------------------------------------

    def make_sim_callback(self, truck_id: str) -> Callable[[Any, Dict[str, Any]], None]:
        """
        Retorna função compatível com o callback esperado pelo Simulator.

        Assinatura do callback do Simulator:
            callback(state: TruckState, sensors: Dict[str, Any]) -> None

        Aqui ignoramos o objeto state e publicamos apenas o dicionário de sensores.
        """

        def _callback(state: Any, sensors: Dict[str, Any]) -> None:
            self.publish_state(truck_id, sensors)

        return _callback
