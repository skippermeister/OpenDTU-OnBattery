// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "WebApi_network.h"
#include "Configuration.h"
#include "ErrorMessages.h"
#include "NetworkSettings.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "helper.h"
#include <AsyncJson.h>

void WebApiNetworkClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/network/status", HTTP_GET, std::bind(&WebApiNetworkClass::onNetworkStatus, this, _1));
    server.on("/api/network/config", HTTP_GET, std::bind(&WebApiNetworkClass::onNetworkAdminGet, this, _1));
    server.on("/api/network/config", HTTP_POST, std::bind(&WebApiNetworkClass::onNetworkAdminPost, this, _1));
}

void WebApiNetworkClass::onNetworkStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();

    root["sta_status"] = ((WiFi.getMode() & WIFI_STA) != 0);
    root["sta_ssid"] = WiFi.SSID();
    root["sta_bssid"] = WiFi.BSSIDstr();
    root["sta_rssi"] = WiFi.RSSI();
    root["network_hostname"] = NetworkSettings.getHostname();
    root["network_ip"] = NetworkSettings.localIP().toString();
    root["network_netmask"] = NetworkSettings.subnetMask().toString();
    root["network_gateway"] = NetworkSettings.gatewayIP().toString();
    root["network_dns1"] = NetworkSettings.dnsIP(0).toString();
    root["network_dns2"] = NetworkSettings.dnsIP(1).toString();
    root["network_mac"] = NetworkSettings.macAddress();
    root["network_mode"] = NetworkSettings.NetworkMode() == network_mode::WiFi ? "Station" : "Ethernet";
    root["ap_status"] = ((WiFi.getMode() & WIFI_AP) != 0);
    root["ap_ssid"] = NetworkSettings.getApName();
    root["ap_ip"] = WiFi.softAPIP().toString();
    root["ap_mac"] = WiFi.softAPmacAddress();
    root["ap_stationnum"] = WiFi.softAPgetStationNum();

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiNetworkClass::onNetworkAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const WiFi_CONFIG_T& cWiFi = Configuration.get().WiFi;

    root["hostname"] = cWiFi.Hostname;
    root["dhcp"] = cWiFi.Dhcp;
    root["ipaddress"] = IPAddress(cWiFi.Ip).toString();
    root["netmask"] = IPAddress(cWiFi.Netmask).toString();
    root["gateway"] = IPAddress(cWiFi.Gateway).toString();
    root["dns1"] = IPAddress(cWiFi.Dns1).toString();
    root["dns2"] = IPAddress(cWiFi.Dns2).toString();
    root["ssid"] = cWiFi.Ssid;
    root["password"] = cWiFi.Password;
    root["aptimeout"] = cWiFi.ApTimeout;
    root["mdnsenabled"] = Configuration.get().Mdns.Enabled;

#ifdef USE_ModbusDTU
    root["froniussmmodbusenabled"] = Configuration.get().Modbus.Fronius_SM_Simulation_Enabled;
#else
    root["froniussmmodbusenabled"] = false;
#endif

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiNetworkClass::onNetworkAdminPost(AsyncWebServerRequest* request)
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

    if (!(root.containsKey("ssid")
            && root.containsKey("password")
            && root.containsKey("hostname")
            && root.containsKey("dhcp")
            && root.containsKey("ipaddress")
            && root.containsKey("netmask")
            && root.containsKey("gateway")
            && root.containsKey("dns1")
            && root.containsKey("dns2")
            && root.containsKey("aptimeout"))) {
        retMsg["message"] = ValuesAreMissing;
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    IPAddress ipaddress;
    if (!ipaddress.fromString(root["ipaddress"].as<String>())) {
        retMsg["message"] = "IP address is invalid!";
        retMsg["code"] = WebApiError::NetworkIpInvalid;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }
    IPAddress netmask;
    if (!netmask.fromString(root["netmask"].as<String>())) {
        retMsg["message"] = "Netmask is invalid!";
        retMsg["code"] = WebApiError::NetworkNetmaskInvalid;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }
    IPAddress gateway;
    if (!gateway.fromString(root["gateway"].as<String>())) {
        retMsg["message"] = "Gateway is invalid!";
        retMsg["code"] = WebApiError::NetworkGatewayInvalid;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }
    IPAddress dns1;
    if (!dns1.fromString(root["dns1"].as<String>())) {
        retMsg["message"] = "DNS Server IP 1 is invalid!";
        retMsg["code"] = WebApiError::NetworkDns1Invalid;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }
    IPAddress dns2;
    if (!dns2.fromString(root["dns2"].as<String>())) {
        retMsg["message"] = "DNS Server IP 2 is invalid!";
        retMsg["code"] = WebApiError::NetworkDns2Invalid;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    if (root["hostname"].as<String>().length() == 0 || root["hostname"].as<String>().length() > WIFI_MAX_HOSTNAME_STRLEN) {
        retMsg["message"] = "Hostname must between 1 and " STR(WIFI_MAX_HOSTNAME_STRLEN) " characters long!";
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }
    if (NetworkSettings.NetworkMode() == network_mode::WiFi) {
        if (root["ssid"].as<String>().length() == 0 || root["ssid"].as<String>().length() > WIFI_MAX_SSID_STRLEN) {
            retMsg["message"] = "SSID must between 1 and " STR(WIFI_MAX_SSID_STRLEN) " characters long!";
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }
    }
    if (root["password"].as<String>().length() > WIFI_MAX_PASSWORD_STRLEN - 1) {
        retMsg["message"] = "Password must not be longer than " STR(WIFI_MAX_PASSWORD_STRLEN) " characters long!";
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }
    if (root["aptimeout"].as<uint>() > 99999) {
        retMsg["message"] = "ApTimeout must be a number between 0 and 99999!";
        retMsg["code"] = WebApiError::NetworkApTimeoutInvalid;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    WiFi_CONFIG_T& cWiFi = Configuration.get().WiFi;
    cWiFi.Ip[0] = ipaddress[0];
    cWiFi.Ip[1] = ipaddress[1];
    cWiFi.Ip[2] = ipaddress[2];
    cWiFi.Ip[3] = ipaddress[3];
    cWiFi.Netmask[0] = netmask[0];
    cWiFi.Netmask[1] = netmask[1];
    cWiFi.Netmask[2] = netmask[2];
    cWiFi.Netmask[3] = netmask[3];
    cWiFi.Gateway[0] = gateway[0];
    cWiFi.Gateway[1] = gateway[1];
    cWiFi.Gateway[2] = gateway[2];
    cWiFi.Gateway[3] = gateway[3];
    cWiFi.Dns1[0] = dns1[0];
    cWiFi.Dns1[1] = dns1[1];
    cWiFi.Dns1[2] = dns1[2];
    cWiFi.Dns1[3] = dns1[3];
    cWiFi.Dns2[0] = dns2[0];
    cWiFi.Dns2[1] = dns2[1];
    cWiFi.Dns2[2] = dns2[2];
    cWiFi.Dns2[3] = dns2[3];
    strlcpy(cWiFi.Ssid, root["ssid"].as<String>().c_str(), sizeof(cWiFi.Ssid));
    strlcpy(cWiFi.Password, root["password"].as<String>().c_str(), sizeof(cWiFi.Password));
    strlcpy(cWiFi.Hostname, root["hostname"].as<String>().c_str(), sizeof(cWiFi.Hostname));
    if (root["dhcp"].as<bool>()) {
        cWiFi.Dhcp = true;
    } else {
        cWiFi.Dhcp = false;
    }
    cWiFi.ApTimeout = root["aptimeout"].as<uint>();

    Mdns_CONFIG_T& cMdns = Configuration.get().Mdns;
    cMdns.Enabled = root["mdnsenabled"].as<bool>();

#ifdef USE_ModbusDTU
    Modbus_CONFIG_T& cModbus = Configuration.get().Modbus;
    cModbus.Fronius_SM_Simulation_Enabled = root["froniussmmodbusenabled"].as<bool>();
#endif

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    NetworkSettings.enableAdminMode();
    NetworkSettings.applyConfig();
}
