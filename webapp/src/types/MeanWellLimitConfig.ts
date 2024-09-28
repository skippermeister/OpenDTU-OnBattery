export interface MeanWellLimitConfig {
    voltage: number;
    voltageValid: boolean;
    current: number;
    currentValid: boolean;
    curveCV: number;
    curveCVvalid: boolean;
    curveCC: number;
    curveCCvalid: boolean;
    curveFV: number;
    curveFVvalid: boolean;
    curveTC: number;
    curveTCvalid: boolean;
}

export interface MeanWellLimitStatus {
    voltage: number;
    current: number;
    curveCV: number;
    curveCC: number;
    curveFV: number;
    curveTC: number;
    online: boolean;
}
