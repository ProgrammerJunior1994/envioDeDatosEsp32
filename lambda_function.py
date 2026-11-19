import json
import boto3
from decimal import Decimal
from datetime import datetime, timezone


# ==========================================
# 1. CONEXIÓN CON DYNAMODB
# ==========================================

dynamodb = boto3.resource("dynamodb")

TABLE_NAME = "rutas"

tabla = dynamodb.Table(TABLE_NAME)


# ==========================================
# 2. RESPUESTA HTTP
# ==========================================

def response(status_code, body):

    return {
        "statusCode": status_code,
        "headers": {
            "Content-Type": "application/json"
        },
        "body": json.dumps(body)
    }


# ==========================================
# 3. FUNCIÓN PRINCIPAL DE LAMBDA
# ==========================================

def lambda_handler(event, context):

    print("Evento recibido:")
    print(json.dumps(event))

    # --------------------------------------
    # 3.1 OBTENER LOS DATOS RECIBIDOS
    # --------------------------------------

    try:

        if "body" in event:

            body = event["body"]

            if isinstance(body, str):
                data = json.loads(body)

            elif isinstance(body, dict):
                data = body

            else:
                return response(400, {
                    "error": "Formato de body no válido"
                })

        else:
            data = event

    except Exception as e:

        print("Error al interpretar JSON:", str(e))

        return response(400, {
            "error": "No se pudo interpretar el JSON"
        })

    # --------------------------------------
    # 3.2 VALIDAR QUE SEA UN OBJETO JSON
    # --------------------------------------

    if not isinstance(data, dict):

        return response(400, {
            "error": "Los datos recibidos deben ser un objeto JSON"
        })

    # --------------------------------------
    # 3.3 VALIDAR CAMPOS OBLIGATORIOS
    # --------------------------------------

    required_fields = [
        "device_id",
        "shipment_id",
        "timestamp",
        "temperature_c",
        "humidity_pct",
        "latitude",
        "longitude",
        "speed_kmh"
    ]

    missing_fields = [
        field for field in required_fields
        if field not in data or data[field] is None
    ]

    if missing_fields:

        return response(400, {
            "error": "Faltan campos obligatorios",
            "missing_fields": missing_fields
        })

    # --------------------------------------
    # 3.4 VALIDAR DATOS NUMÉRICOS
    # --------------------------------------

    try:

        temperature = Decimal(str(data["temperature_c"]))
        humidity = Decimal(str(data["humidity_pct"]))
        latitude = Decimal(str(data["latitude"]))
        longitude = Decimal(str(data["longitude"]))
        speed = Decimal(str(data["speed_kmh"]))

        # Evitar valores NaN o infinitos
        numeric_values = [
            temperature,
            humidity,
            latitude,
            longitude,
            speed
        ]

        if not all(value.is_finite() for value in numeric_values):
            raise ValueError("Se recibieron valores numéricos no válidos")

    except (ValueError, TypeError, ArithmeticError) as e:

        return response(400, {
            "error": "Los campos numéricos contienen valores no válidos",
            "details": str(e)
        })

    # --------------------------------------
    # 3.5 VALIDAR RANGOS
    # --------------------------------------

    if not Decimal("0") <= humidity <= Decimal("100"):

        return response(400, {
            "error": "La humedad debe estar entre 0 y 100"
        })

    if not Decimal("-90") <= latitude <= Decimal("90"):

        return response(400, {
            "error": "Latitud fuera de rango"
        })

    if not Decimal("-180") <= longitude <= Decimal("180"):

        return response(400, {
            "error": "Longitud fuera de rango"
        })

    if speed < 0:

        return response(400, {
            "error": "La velocidad no puede ser negativa"
        })

    # --------------------------------------
    # 3.6 VALIDAR IDENTIFICADORES
    # --------------------------------------

    device_id = str(data["device_id"]).strip()
    shipment_id = str(data["shipment_id"]).strip()
    timestamp = str(data["timestamp"]).strip()

    if not device_id or not shipment_id or not timestamp:

        return response(400, {
            "error": "Los identificadores no pueden estar vacíos"
        })

    # --------------------------------------
    # 3.7 GENERAR ID DEL REGISTRO
    # --------------------------------------

    registro_id = f"{shipment_id}#{timestamp}"

    # --------------------------------------
    # 3.8 PREPARAR REGISTRO PARA DYNAMODB
    # --------------------------------------

    registro = {

        # Clave de partición
        "numCamion": device_id,

        # Clave de ordenamiento
        "id": registro_id,

        # Información del dispositivo
        "device_id": device_id,

        # Información del envío
        "shipment_id": shipment_id,

        # Fecha y hora del registro
        "timestamp": timestamp,

        # Datos del sensor
        "temperature_c": temperature,
        "humidity_pct": humidity,

        # Ubicación GPS
        "latitude": latitude,
        "longitude": longitude,

        # Velocidad
        "speed_kmh": speed,

        # Fecha de recepción en AWS
        "received_at": datetime.now(timezone.utc).isoformat()

    }

    # --------------------------------------
    # 3.9 GUARDAR EN DYNAMODB
    # --------------------------------------

    try:

        tabla.put_item(
            Item=registro
        )

        print("Registro guardado correctamente:")
        print(json.dumps(registro, default=str))

        return response(200, {

            "message": "Datos recibidos y guardados correctamente",

            "device_id": device_id,

            "shipment_id": shipment_id,

            "registro_id": registro_id,

            "received_at": registro["received_at"]

        })

    except Exception as e:

        print("Error al guardar en DynamoDB:")
        print(str(e))

        return response(500, {

            "error": "Error al guardar los datos en DynamoDB",

            "details": str(e)

        })