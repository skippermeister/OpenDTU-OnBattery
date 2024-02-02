import type { ValueObject } from '@/types/LiveDataStatus';

// REFUsol
export interface REFUsol {
    serNo: string;
    PID: string;
    firmware: string;
    age_critical: boolean;
    data_age: 0;
    deviceValues: (ValueObject | string)[];
    acValues: (ValueObject | string)[];
    dcValues: (ValueObject | string)[];
    yieldValues: (ValueObject | string)[];
}