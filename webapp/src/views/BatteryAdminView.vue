<template>
    <BasePage :title="$t('batteryadmin.BatterySettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <form @submit="saveBatteryConfig">
            <CardElement :text="$t('batteryadmin.BatteryConfiguration')" textVariant="text-bg-primary">
                <InputElement :label="$t('batteryadmin.EnableBattery')"
                              v-model="batteryConfigList.enabled"
                              type="checkbox" wide4_1/>

                <div v-show="batteryConfigList.enabled">
                    <InputElement :label="$t('batteryadmin.UpdatesOnly')"
                                  v-model="batteryConfigList.updatesonly"
                                  type="checkbox" wide4_1/>

                    <InputElement :label="$t('batteryadmin.VerboseLogging')"
                                  v-model="batteryConfigList.verbose_logging"
                                  type="checkbox" wide4_1/>
                    <div class="row mb-3">
                        <label class="col-sm-4 col-form-label">
                                {{ $t('batteryadmin.Provider') }}
                        </label>
                        <div class="col-sm-4">
                            <select class="form-select" v-model="batteryConfigList.provider">
                                <option v-for="provider in providerTypeList" :key="provider.key" :value="provider.key">
                                    {{ $t(`batteryadmin.Provider` + provider.value) }}
                                </option>
                            </select>
                        </div>
                    </div>
                </div>
            </CardElement>

            <div v-show="batteryConfigList.enabled">
                <CardElement :text="$t('batteryadmin.BatteryParameter')"
                             textVariant="text-bg-primary"
                             add-space>

                    <InputElement v-show="batteryConfigList.provider == 0 ||
                                          batteryConfigList.provider == 3 ||
                                        batteryConfigList.provider == 5"
                                :label="$t('batteryadmin.PollInterval')"
                                v-model="batteryConfigList.pollinterval"
                                type="number" min="2" max="90" wide4_2
                                :postfix="$t('batteryadmin.Seconds')"/>

                    <InputElement :label="$t('batteryadmin.MinChargeTemp')"
                                v-model="batteryConfigList.min_charge_temp"
                                type="number" step="1" min="-25" max="75" wide4_2
                                :postfix="$t('batteryadmin.Celsius')"/>

                    <InputElement :label="$t('batteryadmin.MaxChargeTemp')"
                                v-model="batteryConfigList.max_charge_temp"
                                type="number" step="1" min="-25" max="75" wide4_2
                                :postfix="$t('batteryadmin.Celsius')"/>

                    <InputElement :label="$t('batteryadmin.MinDischargeTemp')"
                                v-model="batteryConfigList.min_discharge_temp"
                                type="number" step="1" min="-25" max="75" wide4_2
                                :postfix="$t('batteryadmin.Celsius')"/>

                    <InputElement :label="$t('batteryadmin.MaxDischargeTemp')"
                                v-model="batteryConfigList.max_discharge_temp"
                                type="number" step="1" min="-25" max="75" wide4_2
                                :postfix="$t('batteryadmin.Celsius')"/>
                </CardElement>

                <CardElement v-show="batteryConfigList.provider == 2"
                             :text="$t('batteryadmin.CanControllerConfiguration')"
                             textVariant="text-bg-primary"
                             addSpace>
                    <div class="row mb-3">
                        <label class="col-sm-4 col-form-label">
                                {{ $t('batteryadmin.CanControllerFrequency') }}
                        </label>
                        <div class="col-sm-2">
                            <select class="form-select" v-model="batteryConfigList.can_controller_frequency">
                                <option v-for="frequency in frequencyTypeList" :key="frequency.key" :value="frequency.value">
                                    {{ frequency.key }} MHz
                                </option>
                            </select>
                        </div>
                    </div>
                </CardElement>

                <CardElement v-show="batteryConfigList.provider == 3"
                             :text="$t('batteryadmin.JkBmsConfiguration')"
                             textVariant="text-bg-primary"
                             addSpace>
                    <div class="row mb-3">
                        <label class="col-sm-2 col-form-label">
                            {{ $t('batteryadmin.JkBmsInterface') }}
                        </label>
                        <div class="col-sm-3">
                            <select class="form-select" v-model="batteryConfigList.jkbms_interface">
                                <option v-for="jkBmsInterface in jkBmsInterfaceTypeList" :key="jkBmsInterface.key" :value="jkBmsInterface.key">
                                    {{ $t(`batteryadmin.JkBmsInterface` + jkBmsInterface.value) }}
                                </option>
                            </select>
                        </div>
                    </div>
                </CardElement>

                <CardElement v-show="batteryConfigList.enabled && batteryConfigList.provider == 6"
                         :text="$t('batteryadmin.MqttConfiguration')" textVariant="text-bg-primary" addSpace>
                    <div class="row mb-3">
                        <label class="col-sm-2 col-form-label">
                            {{ $t('batteryadmin.MqttSocTopic') }}
                        </label>
                        <div class="col-sm-10">
                            <div class="input-group">
                                <input type="text" class="form-control" v-model="batteryConfigList.mqtt_soc_topic" />
                        </div>
                    </div>
                </div>
                <div class="row mb-3">
                    <label class="col-sm-2 col-form-label">
                        {{ $t('batteryadmin.MqttVoltageTopic') }}
                    </label>
                    <div class="col-sm-10">
                        <div class="input-group">
                            <input type="text" class="form-control" v-model="batteryConfigList.mqtt_voltage_topic" />
                            </div>
                        </div>
                    </div>
                </CardElement>
            </div>

            <FormFooter @reload="getBatteryConfig"/>
        </form>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from "@/components/BootstrapAlert.vue";
import FormFooter from '@/components/FormFooter.vue';
import CardElement from '@/components/CardElement.vue';
import InputElement from '@/components/InputElement.vue';
import type { BatteryConfig } from "@/types/BatteryConfig";
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
            batteryConfigList: {} as BatteryConfig,
            alertMessage: "",
            alertType: "info",
            showAlert: false,
            providerTypeList: [
                { key: 0, value: 'PylontechRS485' },
                { key: 1, value: 'PylontechCan0' },
                { key: 2, value: 'PylontechMCP2515' },
                { key: 3, value: 'JkBmsSerial' },
                { key: 4, value: 'Victron' },
                { key: 5, value: 'DalyBmsRS485' },
                { key: 6, value: 'Mqtt' }
            ],
            jkBmsInterfaceTypeList: [
                { key: 0, value: 'Uart' },
                { key: 1, value: 'Transceiver' },
            ],
            frequencyTypeList: [
                { key:  8, value:  8000000 },
                { key: 16, value: 16000000 },
                { key: 20, value: 20000000 },
            ],
        };
    },
    created() {
        this.getBatteryConfig();
    },
    methods: {
        getBatteryConfig() {
            this.dataLoading = true;
            fetch("/api/battery/config", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.batteryConfigList = data;
                    this.dataLoading = false;
                });
        },
        saveBatteryConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append("data", JSON.stringify(this.batteryConfigList));

            fetch("/api/battery/config", {
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
