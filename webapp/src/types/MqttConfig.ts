export interface MqttConfig {
    enabled: boolean;
    hostname: string;
    port: number;
    clientid: string;
    username: string;
    password: string;
    topic: string;
    publish_interval: number;
    clean_session: boolean;
    retain: boolean;
    tls: boolean;
    root_ca_cert: string;
    tls_cert_login: boolean;
    client_cert: string;
    client_key: string;
    lwt_topic: string;
    lwt_online: string;
    lwt_offline: string;
    lwt_qos: number;
    hass_enabled: boolean;
    hass_expire: boolean;
    hass_retain: boolean;
    hass_topic: string;
    hass_individualpanels: boolean;
    verbose_logging: boolean;
}
