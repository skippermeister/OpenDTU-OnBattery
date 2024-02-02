export interface MqttStatus {
    enabled: boolean;
    hostname: string;
    port: number;
    username: string;
    topic: string;
    publish_interval: number;
    clean_session: boolean;
    retain: boolean;
    tls: boolean;
    root_ca_cert_info: string;
    tls_cert_login: boolean;
    client_cert_info: string;
    connected: boolean;
    hass_enabled: boolean;
    hass_expire: boolean;
    hass_retain: boolean;
    hass_topic: string;
    hass_individualpanels: boolean;
    verbose_logging: boolean;
}
