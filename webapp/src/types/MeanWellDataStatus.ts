import type { ValueObject } from '@/types/LiveDataStatus';

// MeanWell
export interface MeanWell {
    data_age: 0;
    manufacturerModelName: string;
    automatic: boolean;
    inputValues: (ValueObject | string)[];
    operation: boolean;
    cuve: boolean;
    stgs: boolean;
    outputValues: (ValueObject | string)[];
}
