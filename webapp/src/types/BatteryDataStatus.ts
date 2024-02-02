import type { ValueObject, CellObject } from '@/types/LiveDataStatus';

export interface Battery {
    manufacturer: string;
    device_name: string;
    data_age: number;
    software_version: string;
    values: (ValueObject | string)[];
    parameters: (ValueObject | string)[];
    issues: number[];
    cell: CellObject;
    tempSensor: ValueObject[];
}
