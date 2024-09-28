import type { ValueObject, CellObject } from '@/types/LiveDataStatus';

export interface StringValue {
    value: string;
    translate: boolean;
}

export interface Pack {
    moduleNumber: number;
    moduleRole: string;
    moduleName: string;
    swversion: string;
    moduleSerialNumber: string;
    values: (ValueObject | StringValue)[];
    parameters: (ValueObject | StringValue)[];
    cell: CellObject;
    tempSensor: ValueObject[];
}

export interface BatteryData {
    manufacturer: string;
    fwversion: string;
    hwversion: string;
    swversion: string;
    moduleSerialNumber: string;
    data_age: number;
    values: (ValueObject | StringValue)[];
    issues: number[];
    numberOfPacks: number;
    packs: Pack[];
}
