export interface ValueObject {
    v: number; // value
    u: string; // unit
    d: number; // digits
    max: number;
}

export interface ArrayValueObject {
    values: number[]; // values
    u: string; // unit
    d: number; // digits
}

export interface CellObject {
    voltage: ValueObject[],
    cellMinVoltage: ValueObject,
    cellMaxVoltage: ValueObject,
    cellDiffVoltage: ValueObject
}

export interface InverterStatistics {
    name: ValueObject,
    Power?: ValueObject;
    Voltage?: ValueObject;
    Current?: ValueObject;
    "Power DC"?: ValueObject;
    YieldDay?: ValueObject;
    YieldTotal?: ValueObject;
    Frequency?: ValueObject;
    Temperature?: ValueObject;
    PowerFactor?: ValueObject;
    ReactivePower?: ValueObject;
    Efficiency?: ValueObject;
    Irradiation?: ValueObject;
}

export interface Inverter {
    serial: string;
    name: string;
    order: number;
    data_age: number;
    poll_enabled: boolean;
    reachable: boolean;
    producing: boolean;
    limit_relative: number;
    limit_absolute: number;
    events: number;
    AC: InverterStatistics[];
    DC: InverterStatistics[];
    INV: InverterStatistics[];
}

export interface Total {
    Power: ValueObject;
    YieldDay: ValueObject;
    YieldTotal: ValueObject;
}

export interface Hints {
    time_sync: boolean;
    default_password: boolean;
    radio_problem: boolean;
}

export interface REFUsol {
    enabled: boolean;
    total: Total;
}

export interface Vedirect {
    enabled: boolean;
    total: Total;
}

export interface MeanWell {
  enabled: boolean;
  Power: ValueObject;
}

// export interface Huawei {
//   enabled: boolean;
//   Power: ValueObject;
// }

export interface Battery {
  enabled: boolean;
  soc?: ValueObject;
  voltage?: ValueObject;
  power?: ValueObject;
  current?: ValueObject;
}

export interface PowerMeter {
  enabled: boolean;
  GridPower: ValueObject;
  HousePower: ValueObject;
}

export interface LiveData {
    inverters: Inverter[];
    total: Total;
    hours: ArrayValueObject;
    hints: Hints;
    // huawei: Huawei;
    refusol: REFUsol;
    vedirect: Vedirect;
    meanwell: MeanWell;
    battery: Battery;
    power_meter: PowerMeter;
}
