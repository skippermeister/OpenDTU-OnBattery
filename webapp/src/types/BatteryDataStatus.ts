import type { ValueObject, CellObject } from '@/types/LiveDataStatus';

export interface Pack {
    moduleNumber: number;
    moduleName: string;
    device_name: string;
    moduleSerialNumber: string;
    software_version: string;
    values: (ValueObject | string)[];
    parameters: (ValueObject | string)[];
    cell: CellObject;
    tempSensor: ValueObject[];
}

export interface BatteryData {
    manufacturer: string;
    fwversion: string;
    hwversion: string;
    data_age: number;
    values: (ValueObject | string)[];
    issues: number[];
    numberOfPacks: number;
    packs: Pack[];
}
