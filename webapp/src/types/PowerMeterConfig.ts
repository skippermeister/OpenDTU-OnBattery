export interface PowerMeterHttpPhaseConfig {
    index: number;
    enabled: boolean;
    url: string;
    auth_type: number;
    username: string;
    password: string;
    header_key: string;
    header_value: string;
    json_path: string;
    timeout: number;
    unit: number;
}

export interface PowerMeterConfig {
    enabled: boolean;
    pollinterval: number;
    updatesonly: boolean;
    source: number;
    mqtt_topic_powermeter_1: string;
    mqtt_topic_powermeter_2: string;
    mqtt_topic_powermeter_3: string;
    sdmbaudrate: number;
    sdmaddress: number;
    http_individual_requests: boolean;
    http_phases: Array<PowerMeterHttpPhaseConfig>;
    verbose_logging: boolean;
}
