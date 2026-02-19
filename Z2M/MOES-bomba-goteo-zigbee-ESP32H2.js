const {
    battery,
    numeric,
    enumLookup,
    binary,
    deviceEndpoints,
    onOff
} = require('zigbee-herdsman-converters/lib/modernExtend');

const definition = {
    zigbeeModel: ['MOES-bomba-goteo-zigbee-ESP32H2'],
    model: 'MOES-bomba-goteo-zigbee-ESP32H2',
    vendor: 'FSD', // Puedes cambiar esto si lo deseas
    description: 'Controlador de Riego Dual Inteligente con Sensor de Nivel',
    extend: [
        deviceEndpoints({
            endpoints: {
                '1': 1, // Bomba A
                '2': 2, // Bomba B
                '3': 3  // Sensor Nivel & Batería
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
        })
    ],
    meta: { multiEndpoint: true },
};

module.exports = definition;
