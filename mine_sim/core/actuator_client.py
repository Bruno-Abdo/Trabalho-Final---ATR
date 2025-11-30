# core/actuator_client.py
from __future__ import annotations

import json
import logging
import time
from typing import Tuple, Optional

import paho.mqtt.client as mqtt

logger = logging.getLogger(__name__)


class ActuatorMqttClient:
    """
    Cliente MQTT responsável por ASSINAR o tópico de atuadores
    de um caminhão e manter o último comando recebido.

    Tópico:
        atr/truck/<id>/atuadores

    Payload esperado (mesmo formato do publish_actuators):
        {
            "truck_id": "001",
            "timestamp": 1234567890.0,
            "actuators": {
                "o_aceleracao": <int>,
                "o_direcao": <int>
            }
        }
    """

    def __init__(
        self,
        truck_id: str,
        host: str = "localhost",
        port: int = 1883,
        base_topic: str = "atr/truck",
        client_id: Optional[str] = None,
        keepalive: int = 60,
    ) -> None:
        self.truck_id = truck_id
        self.base_topic = base_topic
        self.host = host
        self.port = port
        self.keepalive = keepalive

        self._topic = f"{self.base_topic}/{self.truck_id}/atuadores"

        self._client = mqtt.Client(
            client_id=client_id or f"actuator_sub_{self.truck_id}_{int(time.time())}"
        )

        # Estado interno (último comando recebido)
        self._last_acel: float = 0.0
        self._last_dir: float = 0.0

        # Callbacks do paho-mqtt
        self._client.on_connect = self._on_connect
        self._client.on_message = self._on_message

        self._connected: bool = False

    # ------------------------------------------------------------------
    # Conexão e loop
    # ------------------------------------------------------------------

    def connect(self, start_loop: bool = True) -> None:
        """
        Conecta ao broker e assina o tópico de atuadores.
        Se start_loop=True, inicia loop em thread própria.
        """
        logger.info("Conectando ActuatorMqttClient em %s:%d ...", self.host, self.port)
        self._client.connect(self.host, self.port, self.keepalive)

        if start_loop:
            self._client.loop_start()

    def disconnect(self, stop_loop: bool = True) -> None:
        if stop_loop:
            self._client.loop_stop()
        self._client.disconnect()
        self._connected = False

    # ------------------------------------------------------------------
    # Callbacks paho-mqtt
    # ------------------------------------------------------------------

    def _on_connect(self, client, userdata, flags, rc) -> None:
        if rc == 0:
            logger.info("ActuatorMqttClient conectado, assinando %s", self._topic)
            self._connected = True
            client.subscribe(self._topic)
        else:
            logger.error("Falha ao conectar ActuatorMqttClient (rc=%s)", rc)

    def _on_message(self, client, userdata, msg) -> None:
        try:
            payload_str = msg.payload.decode("utf-8", errors="ignore")
            data = json.loads(payload_str)
        except Exception as exc:  # JSON ou decode
            logger.error("Erro ao decodificar payload de atuadores: %s", exc)
            return

        try:
            acts = data.get("actuators", {})
            acel = float(acts.get("o_aceleracao", 0.0))
            dire = float(acts.get("o_direcao", 0.0))
        except Exception as exc:
            logger.error("Payload de atuadores mal formatado: %s", exc)
            return

        # Atualiza último comando
        self._last_acel = acel
        self._last_dir = dire
        logger.debug(
            "Atuadores recebidos de %s: o_aceleracao=%s, o_direcao=%s",
            self.truck_id,
            acel,
            dire,
        )

    # ------------------------------------------------------------------
    # API para o Simulator
    # ------------------------------------------------------------------

    def get_latest(self) -> Tuple[float, float]:
        """
        Retorna o último comando de atuadores recebido.

        Esta função é thread-safe o suficiente para nosso uso,
        pois a atualização acontece via GIL no callback do paho.
        """
        return self._last_acel, self._last_dir
