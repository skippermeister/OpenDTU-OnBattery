import type { ValueObject } from '@/types/LiveDataStatus';

// Huawei
export interface Huawei {
    data_age: 0;
    inputValues: (ValueObject | string)[];
    outputValues: (ValueObject | string)[];
    amp_hour: ValueObject;
}
