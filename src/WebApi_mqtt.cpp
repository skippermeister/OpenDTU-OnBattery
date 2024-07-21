// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "WebApi_mqtt.h"
#include "Configuration.h"
#include "MqttHandleHass.h"
#include "MqttHandleInverter.h"
#include "MqttHandleVedirectHass.h"
#include "MqttHandleVedirect.h"
#include "MqttSettings.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "helper.h"
#include "PowerLimiter.h"
#include "PowerMeter.h"
#include <AsyncJson.h>

void WebApiMqttClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/mqtt/status", HTTP_GET, std::bind(&WebApiMqttClass::onMqttStatus, this, _1));
    server.on("/api/mqtt/config", HTTP_GET, std::bind(&WebApiMqttClass::onMqttAdminGet, this, _1));
    server.on("/api/mqtt/config", HTTP_POST, std::bind(&WebApiMqttClass::onMqttAdminPost, this, _1));
}

void WebApiMqttClass::onMqttStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const Mqtt_CONFIG_T& cMqtt = Configuration.get().Mqtt;

    root["enabled"] = cMqtt.Enabled;
    root["hostname"] = cMqtt.Hostname;
    root["port"] = cMqtt.Port;
    root["clientid"] = MqttSettings.getClientId();
    root["username"] = cMqtt.Username;
    root["topic"] = cMqtt.Topic;
    root["connected"] = MqttSettings.getConnected();
    root["retain"] = cMqtt.Retain;
    root["tls"] = cMqtt.Tls.Enabled;
    root["root_ca_cert_info"] = getTlsCertInfo(cMqtt.Tls.RootCaCert);
    root["tls_cert_login"] = cMqtt.Tls.CertLogin;
    root["client_cert_info"] = getTlsCertInfo(cMqtt.Tls.ClientCert);
    root["lwt_topic"] = String(cMqtt.Topic) + cMqtt.Lwt.Topic;
    root["publish_interval"] = cMqtt.PublishInterval;
    root["clean_session"] = cMqtt.CleanSession;
#ifdef USE_HASS
    root["hass_enabled"] = cMqtt.Hass.Enabled;
    root["hass_expire"] = cMqtt.Hass.Expire;
    root["hass_retain"] = cMqtt.Hass.Retain;
    root["hass_topic"] = cMqtt.Hass.Topic;
    root["hass_individualpanels"] = cMqtt.Hass.IndividualPanels;
#else
    root["hass_enabled"] = false;
    root["hass_expire"] = false;
    root["hass_retain"] = false;
    root["hass_topic"] = "";
    root["hass_individualpanels"] = false;
#endif
    root["verbose_logging"] = MqttSettings.getVerboseLogging();

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiMqttClass::onMqttAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const Mqtt_CONFIG_T& cMqtt = Configuration.get().Mqtt;

    root["enabled"] = cMqtt.Enabled;
    root["hostname"] = cMqtt.Hostname;
    root["port"] = cMqtt.Port;
    root["clientid"] = cMqtt.ClientId;
    root["username"] = cMqtt.Username;
    root["password"] = cMqtt.Password;
    root["topic"] = cMqtt.Topic;
    root["retain"] = cMqtt.Retain;
    root["tls"] = cMqtt.Tls.Enabled;
    root["root_ca_cert"] = cMqtt.Tls.RootCaCert;
    root["tls_cert_login"] = cMqtt.Tls.CertLogin;
    root["client_cert"] = cMqtt.Tls.ClientCert;
    root["client_key"] = cMqtt.Tls.ClientKey;
    root["lwt_topic"] = cMqtt.Lwt.Topic;
    root["lwt_online"] = cMqtt.Lwt.Value_Online;
    root["lwt_offline"] = cMqtt.Lwt.Value_Offline;
    root["lwt_qos"] = cMqtt.Lwt.Qos;
    root["publish_interval"] = cMqtt.PublishInterval;
    root["clean_session"] = cMqtt.CleanSession;
#ifdef USE_HASS
    root["hass_enabled"] = cMqtt.Hass.Enabled;
    root["hass_expire"] = cMqtt.Hass.Expire;
    root["hass_retain"] = cMqtt.Hass.Retain;
    root["hass_topic"] = cMqtt.Hass.Topic;
    root["hass_individualpanels"] = cMqtt.Hass.IndividualPanels;
#else
    root["hass_enabled"] = false;
    root["hass_expire"] = false;
    root["hass_retain"] = false;
    root["hass_topic"] = "";
    root["hass_individualpanels"] = false;
#endif
    root["verbose_logging"] = MqttSettings.getVerboseLogging();

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);;
}

void WebApiMqttClass::onMqttAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    if (!(root.containsKey("enabled")
            && root.containsKey("hostname")
            && root.containsKey("port")
            && root.containsKey("clientid")
            && root.containsKey("username")
            && root.containsKey("password")
            && root.containsKey("topic")
            && root.containsKey("retain")
            && root.containsKey("tls")
            && root.containsKey("tls_cert_login")
            && root.containsKey("client_cert")
            && root.containsKey("client_key")
            && root.containsKey("lwt_topic")
            && root.containsKey("lwt_online")
            && root.containsKey("lwt_offline")
            && root.containsKey("lwt_qos")
            && root.containsKey("publish_interval")
            && root.containsKey("clean_session")
#ifdef USE_HASS
            && root.containsKey("hass_enabled")
            && root.containsKey("hass_expire")
            && root.containsKey("hass_retain")
            && root.containsKey("hass_topic")
            && root.containsKey("hass_individualpanels")
#endif
            && root.containsKey("verbose_logging"))) {
        retMsg["message"] = ValuesAreMissing;
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    if (root["enabled"].as<bool>()) {
        if (root["hostname"].as<String>().length() == 0 || root["hostname"].as<String>().length() > MQTT_MAX_HOSTNAME_STRLEN) {
            retMsg["message"] = "MqTT Server must between 1 and " STR(MQTT_MAX_HOSTNAME_STRLEN) " characters long!";
            retMsg["code"] = WebApiError::MqttHostnameLength;
            retMsg["param"]["max"] = MQTT_MAX_HOSTNAME_STRLEN;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        if (root["clientid"].as<String>().length() > MQTT_MAX_CLIENTID_STRLEN) {
            retMsg["message"] = "Client ID must not be longer than " STR(MQTT_MAX_CLIENTID_STRLEN) " characters!";
            retMsg["code"] = WebApiError::MqttClientIdLength;
            retMsg["param"]["max"] = MQTT_MAX_CLIENTID_STRLEN;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }
        if (root["username"].as<String>().length() > MQTT_MAX_USERNAME_STRLEN) {
            retMsg["message"] = "Username must not be longer than " STR(MQTT_MAX_USERNAME_STRLEN) " characters!";
            retMsg["code"] = WebApiError::MqttUsernameLength;
            retMsg["param"]["max"] = MQTT_MAX_USERNAME_STRLEN;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }
        if (root["password"].as<String>().length() > MQTT_MAX_PASSWORD_STRLEN) {
            retMsg["message"] = "Password must not be longer than " STR(MQTT_MAX_PASSWORD_STRLEN) " characters!";
            retMsg["code"] = WebApiError::MqttPasswordLength;
            retMsg["param"]["max"] = MQTT_MAX_PASSWORD_STRLEN;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }
        if (root["topic"].as<String>().length() > MQTT_MAX_TOPIC_STRLEN) {
            retMsg["message"] = "Topic must not be longer than " STR(MQTT_MAX_TOPIC_STRLEN) " characters!";
            retMsg["code"] = WebApiError::MqttTopicLength;
            retMsg["param"]["max"] = MQTT_MAX_TOPIC_STRLEN;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        if (root["topic"].as<String>().indexOf(' ') != -1) {
            retMsg["message"] = "Topic must not contain space characters!";
            retMsg["code"] = WebApiError::MqttTopicCharacter;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        if (!root["topic"].as<String>().endsWith("/")) {
            retMsg["message"] = "Topic must end with a slash (/)!";
            retMsg["code"] = WebApiError::MqttTopicTrailingSlash;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        if (root["port"].as<uint>() == 0 || root["port"].as<uint>() > 65535) {
            retMsg["message"] = "Port must be a number between 1 and 65535!";
            retMsg["code"] = WebApiError::MqttPort;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        if (root["root_ca_cert"].as<String>().length() > MQTT_MAX_CERT_STRLEN
            || root["client_cert"].as<String>().length() > MQTT_MAX_CERT_STRLEN
            || root["client_key"].as<String>().length() > MQTT_MAX_CERT_STRLEN) {
            retMsg["message"] = "Certificates must not be longer than " STR(MQTT_MAX_CERT_STRLEN) " characters!";
            retMsg["code"] = WebApiError::MqttCertificateLength;
            retMsg["param"]["max"] = MQTT_MAX_CERT_STRLEN;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        if (root["lwt_topic"].as<String>().length() > MQTT_MAX_TOPIC_STRLEN) {
            retMsg["message"] = "LWT topic must not be longer than " STR(MQTT_MAX_TOPIC_STRLEN) " characters!";
            retMsg["code"] = WebApiError::MqttLwtTopicLength;
            retMsg["param"]["max"] = MQTT_MAX_TOPIC_STRLEN;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        if (root["lwt_topic"].as<String>().indexOf(' ') != -1) {
            retMsg["message"] = "LWT topic must not contain space characters!";
            retMsg["code"] = WebApiError::MqttLwtTopicCharacter;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        if (root["lwt_online"].as<String>().length() > MQTT_MAX_LWTVALUE_STRLEN) {
            retMsg["message"] = "LWT online value must not be longer than " STR(MQTT_MAX_LWTVALUE_STRLEN) " characters!";
            retMsg["code"] = WebApiError::MqttLwtOnlineLength;
            retMsg["param"]["max"] = MQTT_MAX_LWTVALUE_STRLEN;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        if (root["lwt_offline"].as<String>().length() > MQTT_MAX_LWTVALUE_STRLEN) {
            retMsg["message"] = "LWT offline value must not be longer than " STR(MQTT_MAX_LWTVALUE_STRLEN) " characters!";
            retMsg["code"] = WebApiError::MqttLwtOfflineLength;
            retMsg["param"]["max"] = MQTT_MAX_LWTVALUE_STRLEN;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        if (root["lwt_qos"].as<uint8_t>() > 2) {
            retMsg["message"] = "LWT QoS must not be greater than " STR(2) "!";
            retMsg["code"] = WebApiError::MqttLwtQos;
            retMsg["param"]["max"] = 2;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        if (root["publish_interval"].as<uint32_t>() < 5 || root["publish_interval"].as<uint32_t>() > 65535) {
            retMsg["message"] = "Publish interval must be a number between 5 and 65535!";
            retMsg["code"] = WebApiError::MqttPublishInterval;
            retMsg["param"]["min"] = 5;
            retMsg["param"]["max"] = 65535;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

#ifdef USE_HASS
        if (root["hass_enabled"].as<bool>()) {
            if (root["hass_topic"].as<String>().length() > MQTT_MAX_TOPIC_STRLEN) {
                retMsg["message"] = "Hass topic must not be longer than " STR(MQTT_MAX_TOPIC_STRLEN) " characters!";
                retMsg["code"] = WebApiError::MqttHassTopicLength;
                retMsg["param"]["max"] = MQTT_MAX_TOPIC_STRLEN;
                WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                return;
            }

            if (root["hass_topic"].as<String>().indexOf(' ') != -1) {
                retMsg["message"] = "Hass topic must not contain space characters!";
                retMsg["code"] = WebApiError::MqttHassTopicCharacter;
                WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                return;
            }
        }
#endif
    }

    Mqtt_CONFIG_T& cMqtt = Configuration.get().Mqtt;
    cMqtt.Enabled = root["enabled"].as<bool>();
    cMqtt.Retain = root["retain"].as<bool>();
    cMqtt.Tls.Enabled = root["tls"].as<bool>();
    strlcpy(cMqtt.Tls.RootCaCert, root["root_ca_cert"].as<String>().c_str(), sizeof(cMqtt.Tls.RootCaCert));
    cMqtt.Tls.CertLogin = root["tls_cert_login"].as<bool>();
    strlcpy(cMqtt.Tls.ClientCert, root["client_cert"].as<String>().c_str(), sizeof(cMqtt.Tls.ClientCert));
    strlcpy(cMqtt.Tls.ClientKey, root["client_key"].as<String>().c_str(), sizeof(cMqtt.Tls.ClientKey));
    cMqtt.Port = root["port"].as<uint>();
    strlcpy(cMqtt.Hostname, root["hostname"].as<String>().c_str(), sizeof(cMqtt.Hostname));
    strlcpy(cMqtt.ClientId, root["clientid"].as<String>().c_str(), sizeof(cMqtt.ClientId));
    strlcpy(cMqtt.Username, root["username"].as<String>().c_str(), sizeof(cMqtt.Username));
    strlcpy(cMqtt.Password, root["password"].as<String>().c_str(), sizeof(cMqtt.Password));
    strlcpy(cMqtt.Lwt.Topic, root["lwt_topic"].as<String>().c_str(), sizeof(cMqtt.Lwt.Topic));
    strlcpy(cMqtt.Lwt.Value_Online, root["lwt_online"].as<String>().c_str(), sizeof(cMqtt.Lwt.Value_Online));
    strlcpy(cMqtt.Lwt.Value_Offline, root["lwt_offline"].as<String>().c_str(), sizeof(cMqtt.Lwt.Value_Offline));
    cMqtt.Lwt.Qos = root["lwt_qos"].as<uint8_t>();
    cMqtt.PublishInterval = root["publish_interval"].as<uint32_t>();
    cMqtt.CleanSession = root["clean_session"].as<bool>();
#ifdef USE_HASS
    cMqtt.Hass.Enabled = root["hass_enabled"].as<bool>();
    cMqtt.Hass.Expire = root["hass_expire"].as<bool>();
    cMqtt.Hass.Retain = root["hass_retain"].as<bool>();
    cMqtt.Hass.IndividualPanels = root["hass_individualpanels"].as<bool>();
    strlcpy(cMqtt.Hass.Topic, root["hass_topic"].as<String>().c_str(), sizeof(cMqtt.Hass.Topic));
#endif

    MqttSettings.setVerboseLogging(root["verbose_logging"].as<bool>());
    // Check if base topic was changed
    if (strcmp(cMqtt.Topic, root["mqtt_topic"].as<String>().c_str())) {
        MqttHandleInverter.unsubscribeTopics();
        strlcpy(cMqtt.Topic, root["mqtt_topic"].as<String>().c_str(), sizeof(cMqtt.Topic));
        MqttHandleInverter.subscribeTopics();
    }

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    MqttSettings.performReconnect();
#ifdef USE_HASS
    MqttHandleHass.forceUpdate();
    MqttHandleVedirectHass.forceUpdate();
    MqttHandleVedirect.forceUpdate();
#endif
}

String WebApiMqttClass::getTlsCertInfo(const char* cert)
{
    char tlsCertInfo[1024] = "";

    mbedtls_x509_crt tlsCert;

    strlcpy(tlsCertInfo, "Can't parse TLS certificate", sizeof(tlsCertInfo));

    mbedtls_x509_crt_init(&tlsCert);
    int ret = mbedtls_x509_crt_parse(&tlsCert, const_cast<unsigned char*>((unsigned char*)cert), 1 + strlen(cert));
    if (ret < 0) {
        snprintf(tlsCertInfo, sizeof(tlsCertInfo), "Can't parse TLS certificate: mbedtls_x509_crt_parse returned -0x%x\n\n", -ret);
        mbedtls_x509_crt_free(&tlsCert);
        return "";
    }
    mbedtls_x509_crt_info(tlsCertInfo, sizeof(tlsCertInfo) - 1, "", &tlsCert);
    mbedtls_x509_crt_free(&tlsCert);

    return tlsCertInfo;
}
