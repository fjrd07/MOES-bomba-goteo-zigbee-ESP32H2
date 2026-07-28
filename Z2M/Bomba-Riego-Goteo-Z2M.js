const {
    battery,
    numeric,
    enumLookup,
    binary,
    deviceEndpoints,
    deviceAddCustomCluster,
    onOff,
    text,
    ota
} = require('zigbee-herdsman-converters/lib/modernExtend');

// Clúster manufacturer-specific de configuración del programador de riego, implementado en
// src/zb_lib/zb_scheduler_lib.cpp (endpoint 4). 0xFC00 está en el rango reservado por la
// especificación ZCL para clústeres no estándar. Los IDs de atributo deben coincidir
// exactamente con ATTR_* en zb_scheduler_lib.cpp.
//
// ⚠️ VERIFICAR: `deviceAddCustomCluster` es el mecanismo documentado de
// zigbee-herdsman-converters para registrar clústeres manufacturer-specific, pero el nombre
// exacto del helper puede variar entre versiones - si Z2M falla al cargar este convertidor,
// revisa `zigbee-herdsman-converters/lib/modernExtend.js` en tu instalación de Z2M para el
// nombre/firma actual (buscar "CustomCluster" o "addCustomCluster").
const RIEGO_CONFIG_CLUSTER = 'riegoConfig';

const definition = {
    zigbeeModel: ['Bomba-Riego-Goteo-Z2M'],
    model: 'Bomba-Riego-Goteo-Z2M',
    vendor: 'FSD', // Puedes cambiar esto si lo deseas
    description: 'Controlador de Riego Dual Inteligente con Sensor de Nivel y Programador de Riego',
    extend: [
        deviceEndpoints({
            endpoints: {
                '1': 1, // Bomba A
                '2': 2, // Bomba B
                '3': 3, // Sensor Nivel & Batería
                '4': 4  // Configuración del programador de riego
            }
        }),
        onOff({ endpointNames: ['1', '2'] }),
        binary({
            name: 'water_level',
            valueOn: ['LOW', true],
            valueOff: ['OK', false],
            access: 'STATE_GET',
            endpointName: '3',
            description: 'Estado del nivel de agua: OK o BAJO (LOW)'
        }),
        battery({ endpointName: '3' }),
        // Lock safety podría implementarse como un switch de solo lectura o binary
        binary({
            name: 'lock_safety',
            valueOn: ['LOCKED', true],
            valueOff: ['UNLOCKED', false],
            access: 'STATE_GET',
            endpointName: '3',
            description: 'Bloqueo de seguridad por falta de agua'
        }),
        // Habilita OTA vía Z2M. Requiere una entrada en el índice OTA local de Z2M
        // (ver README.md, sección "Actualizar firmware por OTA") apuntando al
        // manufacturerCode/imageType que genera extra_script.py.
        ota(),

        // --- Programador de riego (endpoint 4) - ver docs/hardware/IRRIGATION_SCHEDULING.md ---
        deviceAddCustomCluster(RIEGO_CONFIG_CLUSTER, {
            ID: 0xFC00,
            attributes: {
                pumpAName: { ID: 0x0000, type: 0x42 },   // Character String
                pumpBName: { ID: 0x0001, type: 0x42 },   // Character String
                pumpASchedule: { ID: 0x0002, type: 0x44 }, // Long Character String (JSON)
                pumpBSchedule: { ID: 0x0003, type: 0x44 }, // Long Character String (JSON)
                timezone: { ID: 0x0004, type: 0x42 },    // Character String (POSIX TZ)
                epochTime: { ID: 0x0005, type: 0x23 },   // Unsigned 32-bit Integer
            },
            commands: {},
            commandsResponse: {},
        }),
        text({
            name: 'pump_a_name',
            cluster: RIEGO_CONFIG_CLUSTER,
            attribute: 'pumpAName',
            endpointName: '4',
            description: 'Nombre personalizado de la Bomba A (ej. "Tomates")',
            access: 'ALL',
        }),
        text({
            name: 'pump_b_name',
            cluster: RIEGO_CONFIG_CLUSTER,
            attribute: 'pumpBName',
            endpointName: '4',
            description: 'Nombre personalizado de la Bomba B',
            access: 'ALL',
        }),
        text({
            name: 'pump_a_schedule',
            cluster: RIEGO_CONFIG_CLUSTER,
            attribute: 'pumpASchedule',
            endpointName: '4',
            description: 'Horario de la Bomba A - array JSON de slots, ver docs/hardware/IRRIGATION_SCHEDULING.md',
            access: 'ALL',
        }),
        text({
            name: 'pump_b_schedule',
            cluster: RIEGO_CONFIG_CLUSTER,
            attribute: 'pumpBSchedule',
            endpointName: '4',
            description: 'Horario de la Bomba B - array JSON de slots, ver docs/hardware/IRRIGATION_SCHEDULING.md',
            access: 'ALL',
        }),
        text({
            name: 'timezone',
            cluster: RIEGO_CONFIG_CLUSTER,
            attribute: 'timezone',
            endpointName: '4',
            description: 'Zona horaria POSIX (ej. "COT5" = Colombia UTC-5 sin horario de verano)',
            access: 'ALL',
        }),
        numeric({
            name: 'set_time',
            cluster: RIEGO_CONFIG_CLUSTER,
            attribute: 'epochTime',
            endpointName: '4',
            description: 'Escribe la hora actual en segundos Unix (epoch) para sincronizar el reloj interno del dispositivo - no tiene RTC con batería propia',
            access: 'SET',
            valueMin: 0,
        }),
    ],
    meta: { multiEndpoint: true },
};

module.exports = definition;
