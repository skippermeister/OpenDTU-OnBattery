<template>
    <BasePage :title="$t('mqttadmin.MqttSettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <form @submit="saveMqttConfig">
            <CardElement :text="$t('mqttadmin.MqttConfiguration')" textVariant="text-bg-primary">
                <InputElement :label="$t('mqttadmin.EnableMqtt')"
                              v-model="mqttConfigList.enabled"
                              type="checkbox" wide4_1/>

                <InputElement v-show="mqttConfigList.enabled"
                              :label="$t('mqttadmin.EnableHass')"
                              v-model="mqttConfigList.hass_enabled"
                              type="checkbox" wide4_1/>

                <InputElement v-show="mqttConfigList.enabled"
                              :label="$t('mqttadmin.VerboseLogging')"
                              v-model="mqttConfigList.verbose_logging"
                              type="checkbox" wide4_2/>
            </CardElement>

            <CardElement :text="$t('mqttadmin.MqttBrokerParameter')" textVariant="text-bg-primary" add-space
                         v-show="mqttConfigList.enabled">
                <InputElement :label="$t('mqttadmin.Hostname')"
                              v-model="mqttConfigList.hostname"
                              type="text" maxlength="128" wide3_4
                              :placeholder="$t('mqttadmin.HostnameHint')"/>

                <InputElement :label="$t('mqttadmin.Port')"
                              v-model="mqttConfigList.port"
                              type="number" min="1" max="65535" wide3_2/>

                <InputElement :label="$t('mqttadmin.ClientId')"
                              v-model="mqttConfigList.clientid"
                              type="text" maxlength="64" wide3_5/>

                <InputElement :label="$t('mqttadmin.Username')"
                              v-model="mqttConfigList.username"
                              type="text" maxlength="64" wide3_5
                              :placeholder="$t('mqttadmin.UsernameHint')"/>

                <InputElement :label="$t('mqttadmin.Password')"
                              v-model="mqttConfigList.password"
                              type="password" maxlength="64" wide3_5
                              :placeholder="$t('mqttadmin.PasswordHint')"/>

                <InputElement :label="$t('mqttadmin.BaseTopic')"
                              v-model="mqttConfigList.topic"
                              type="text" maxlength="32" wide3_4
                              :placeholder="$t('mqttadmin.BaseTopicHint')"/>

                <InputElement :label="$t('mqttadmin.PublishInterval')"
                              v-model="mqttConfigList.publish_interval"
                              type="number" min="5" max="86400" wide3_2
                              :postfix="$t('mqttadmin.Seconds')"/>

                <InputElement :label="$t('mqttadmin.CleanSession')"
                              v-model="mqttConfigList.clean_session"
                              type="checkbox" wide3_1/>

                <InputElement :label="$t('mqttadmin.EnableRetain')"
                              v-model="mqttConfigList.retain"
                              type="checkbox" wide3_1/>

                <InputElement :label="$t('mqttadmin.EnableTls')"
                              v-model="mqttConfigList.tls"
                              type="checkbox" wide3_1/>

                <InputElement v-show="mqttConfigList.tls"
                              :label="$t('mqttadmin.RootCa')"
                              v-model="mqttConfigList.root_ca_cert"
                              type="textarea" maxlength="2560" rows="10" wide2_10/>

                <InputElement v-show="mqttConfigList.tls"
                              :label="$t('mqttadmin.TlsCertLoginEnable')"
                              v-model="mqttConfigList.tls_cert_login"
                              type="checkbox" wide2_1/>

                <InputElement v-show="mqttConfigList.tls_cert_login"
                              :label="$t('mqttadmin.ClientCert')"
                              v-model="mqttConfigList.client_cert"
                              type="textarea" maxlength="2560" rows="10" wide2_10/>

                <InputElement v-show="mqttConfigList.tls_cert_login"
                              :label="$t('mqttadmin.ClientKey')"
                              v-model="mqttConfigList.client_key"
                              type="textarea" maxlength="2560" rows="10" wide2_10/>
            </CardElement>

            <CardElement :text="$t('mqttadmin.LwtParameters')" textVariant="text-bg-primary" add-space
                         v-show="mqttConfigList.enabled">
                <InputElement :label="$t('mqttadmin.LwtTopic')"
                              v-model="mqttConfigList.lwt_topic"
                              type="text" maxlength="32" :prefix="mqttConfigList.topic" wide3_4
                              :placeholder="$t('mqttadmin.LwtTopicHint')"/>

                <InputElement :label="$t('mqttadmin.LwtOnline')"
                              v-model="mqttConfigList.lwt_online"
                              type="text" maxlength="20" wide3_4
                              :placeholder="$t('mqttadmin.LwtOnlineHint')"/>

                <InputElement :label="$t('mqttadmin.LwtOffline')"
                              v-model="mqttConfigList.lwt_offline"
                              type="text" maxlength="20" wide3_4
                              :placeholder="$t('mqttadmin.LwtOfflineHint')"/>

            <div class="row mb-3">
                    <label class="col-sm-2 col-form-label">
                        {{ $t('mqttadmin.LwtQos') }}
                    </label>
                    <div class="col-sm-10">
                        <select class="form-select" v-model="mqttConfigList.lwt_qos">
                            <option v-for="qostype in qosTypeList" :key="qostype.key" :value="qostype.key">
                                {{ $t(`mqttadmin.` + qostype.value) }}
                            </option>
                        </select>
                    </div>
                </div>
            </CardElement>

            <CardElement :text="$t('mqttadmin.HassParameters')" textVariant="text-bg-primary" add-space
                         v-show="mqttConfigList.enabled && mqttConfigList.hass_enabled">
                <InputElement :label="$t('mqttadmin.HassPrefixTopic')"
                              v-model="mqttConfigList.hass_topic"
                              type="text" maxlength="32" wide3_4
                              :placeholder="$t('mqttadmin.HassPrefixTopicHint')"/>

                <InputElement :label="$t('mqttadmin.HassRetain')"
                              v-model="mqttConfigList.hass_retain"
                              type="checkbox" placeholder="Hass" wide3_1/>

                <InputElement :label="$t('mqttadmin.HassExpire')"
                              v-model="mqttConfigList.hass_expire"
                              type="checkbox" wide3_1/>

                <InputElement :label="$t('mqttadmin.HassIndividual')"
                              v-model="mqttConfigList.hass_individualpanels"
                              type="checkbox" wide3_1/>
            </CardElement>

            <FormFooter @reload="getMqttConfig"/>
        </form>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from "@/components/BootstrapAlert.vue";
import FormFooter from '@/components/FormFooter.vue';
import CardElement from '@/components/CardElement.vue';
import InputElement from '@/components/InputElement.vue';
import type { MqttConfig } from "@/types/MqttConfig";
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent } from 'vue';

export default defineComponent({
    components: {
        BasePage,
        BootstrapAlert,
        FormFooter,
        CardElement,
        InputElement,
    },
    data() {
        return {
            dataLoading: true,
            mqttConfigList: {} as MqttConfig,
            alertMessage: "",
            alertType: "info",
            showAlert: false,
            qosTypeList: [
                { key: 0, value: 'QOS0' },
                { key: 1, value: 'QOS1' },
                { key: 2, value: 'QOS2' },
            ],
        };
    },
    created() {
        this.getMqttConfig();
    },
    methods: {
        getMqttConfig() {
            this.dataLoading = true;
            fetch("/api/mqtt/config", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.mqttConfigList = data;
                    this.dataLoading = false;
                });
        },
        saveMqttConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append("data", JSON.stringify(this.mqttConfigList));

            fetch("/api/mqtt/config", {
                method: "POST",
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then(
                    (response) => {
                        this.alertMessage = this.$t('apiresponse.' + response.code, response.param);
                        this.alertType = response.type;
                        this.showAlert = true;
                    }
                );
        },
    },
});
</script>
