<template>
    <BasePage :title="$t('batteryadmin.BatterySettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <form @submit="saveBatteryConfig">
            <CardElement :text="$t('batteryadmin.BatteryConfiguration')" textVariant="text-bg-primary">
                <InputElement :label="$t('batteryadmin.EnableBattery')" v-model="batteryConfigList.enabled"
                    type="checkbox" wide4_1 />

                <div v-show="batteryConfigList.enabled">
                    <InputElement :label="$t('batteryadmin.UpdatesOnly')" v-model="batteryConfigList.updatesonly"
                        type="checkbox" wide4_1 />

                    <InputElement :label="$t('batteryadmin.VerboseLogging')" v-model="batteryConfigList.verbose_logging"
                        type="checkbox" wide4_1 />
                    <div class="row mb-3">
                        <label class="col-sm-4 col-form-label">
                            {{ $t('batteryadmin.Provider')}} {{ getIoProvider() }} {{ $t('batteryadmin.Interface')}}
                        </label>
                        <div class="col-sm-4">
                            <select class="form-select" v-model="batteryConfigList.provider">
                                <option v-for="provider in providerTypeList" :key="provider.key" :value="provider.key">
                                    {{ $t(`batteryadmin.Provider` + provider.value) }}
                                </option>
                            </select>
                        </div>
                    </div>

                    <InputElement v-show="batteryConfigList.provider < 5"
                        :label="$t('batteryadmin.NumberOfBatteries')"
                        v-model="batteryConfigList.numberOfBatteries" type="number" min="1" max="5" wide4_1 />
                </div>
            </CardElement>

            <div v-show="batteryConfigList.enabled">
                <CardElement :text="$t('batteryadmin.BatteryParameter')" textVariant="text-bg-primary" add-space>

                    <InputElement v-show="batteryConfigList.provider < 5"
                        :label="$t('batteryadmin.PollInterval')"
                        v-model="batteryConfigList.pollinterval" type="number" min="2" max="90" wide4_2
                        :postfix="$t('batteryadmin.Seconds')" />

                    <InputElement :label="$t('batteryadmin.MinChargeTemp')" v-model="batteryConfigList.min_charge_temp"
                        type="number" step="1" min="-25" max="75" wide4_2 :postfix="$t('batteryadmin.Celsius')" />

                    <InputElement :label="$t('batteryadmin.MaxChargeTemp')" v-model="batteryConfigList.max_charge_temp"
                        type="number" step="1" min="-25" max="75" wide4_2 :postfix="$t('batteryadmin.Celsius')" />

                    <InputElement :label="$t('batteryadmin.MinDischargeTemp')"
                        v-model="batteryConfigList.min_discharge_temp" type="number" step="1" min="-25" max="75" wide4_2
                        :postfix="$t('batteryadmin.Celsius')" />

                    <InputElement :label="$t('batteryadmin.MaxDischargeTemp')"
                        v-model="batteryConfigList.max_discharge_temp" type="number" step="1" min="-25" max="75" wide4_2
                        :postfix="$t('batteryadmin.Celsius')" />

                    <InputElement :label="$t('batteryadmin.StopChargingSoC')"
                        v-model="batteryConfigList.stop_charging_soc" type="number" step="1" min="20" max="100" wide4_2
                        :tooltip="$t('batteryadmin.StopChargingSoCHint')" :postfix="$t('batteryadmin.Percent')" />

                    <InputElement v-show="batteryConfigList.provider == 5 ||
                        batteryConfigList.provider == 6"
                        :label="$t('batteryadmin.RecommendedChargeVoltage')"
                        v-model="batteryConfigList.recommended_charge_voltage" type="number" min="21" max="80"
                        step="0.1" wide4_2 :postfix="$t('batteryadmin.Volts')" />

                    <InputElement v-show="batteryConfigList.provider == 5 ||
                        batteryConfigList.provider == 6"
                        :label="$t('batteryadmin.RecommendedDischargeVoltage')"
                        v-model="batteryConfigList.recommended_discharge_voltage" type="number" min="21" max="80"
                        step="0.1" wide4_2 :postfix="$t('batteryadmin.Volts')" />

                </CardElement>

                <CardElement v-show="batteryConfigList.io_providername == 'MCP2515'"
                    :text="$t('batteryadmin.CanControllerConfiguration')" textVariant="text-bg-primary" addSpace>
                    <div class="row mb-3">
                        <label class="col-sm-4 col-form-label">
                            {{ $t('batteryadmin.CanControllerFrequency') }}
                        </label>
                        <div class="col-sm-2">
                            <select class="form-select" v-model="batteryConfigList.can_controller_frequency">
                                <option v-for="frequency in frequencyTypeList" :key="frequency.key"
                                    :value="frequency.value">
                                    {{ frequency.key }} MHz
                                </option>
                            </select>
                        </div>
                    </div>
                </CardElement>

                <CardElement v-show="batteryConfigList.provider == 3" :text="$t('batteryadmin.JkBmsConfiguration')"
                    textVariant="text-bg-primary" addSpace>
                    <div class="row mb-3">
                        <label class="col-sm-2 col-form-label">
                            {{ $t('batteryadmin.JkBmsInterface') }}
                        </label>
                        <div class="col-sm-3">
                            <select class="form-select" v-model="batteryConfigList.jkbms_interface">
                                <option v-for="jkBmsInterface in jkBmsInterfaceTypeList" :key="jkBmsInterface.key"
                                    :value="jkBmsInterface.key">
                                    {{ $t(`batteryadmin.JkBmsInterface` + jkBmsInterface.value) }}
                                </option>
                            </select>
                        </div>
                    </div>
                </CardElement>

                <template v-if="batteryConfigList.enabled && batteryConfigList.provider == 6">
                    <CardElement :text="$t('batteryadmin.MqttSocConfiguration')" textVariant="text-bg-primary" addSpace>

                        <InputElement :label="$t('batteryadmin.MqttSocTopic')"
                            v-model="batteryConfigList.mqtt_soc_topic" type="text" maxlength="256" />

                        <InputElement :label="$t('batteryadmin.MqttJsonPath')"
                            v-model="batteryConfigList.mqtt_soc_json_path" type="text" maxlength="128"
                            :tooltip="$t('batteryadmin.MqttJsonPathDescription')" />

                    </CardElement>

                    <CardElement :text="$t('batteryadmin.MqttVoltageConfiguration')" textVariant="text-bg-primary"
                        addSpace>

                        <InputElement :label="$t('batteryadmin.MqttVoltageTopic')"
                            v-model="batteryConfigList.mqtt_voltage_topic" type="text" maxlength="256" />

                        <InputElement :label="$t('batteryadmin.MqttJsonPath')"
                            v-model="batteryConfigList.mqtt_voltage_json_path" type="text" maxlength="128"
                            :tooltip="$t('batteryadmin.MqttJsonPathDescription')" />

                        <div class="row mb-3">
                            <label for="mqtt_voltage_unit" class="col-sm-2 col-form-label">
                                {{ $t('batteryadmin.MqttVoltageUnit') }}
                            </label>
                            <div class="col-sm-1">
                                <select id="mqtt_voltage_unit" class="form-select"
                                    v-model="batteryConfigList.mqtt_voltage_unit">
                                    <option v-for="u in voltageUnitTypeList" :key="u.key" :value="u.key">
                                        {{ u.value }}
                                    </option>
                                </select>
                            </div>
                        </div>
                    </CardElement>
                </template>
            </div>

            <FormFooter @reload="getBatteryConfig" />
        </form>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import FormFooter from '@/components/FormFooter.vue';
import CardElement from '@/components/CardElement.vue';
import InputElement from '@/components/InputElement.vue';
import type { BatteryConfig } from '@/types/BatteryConfig';
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
                { key: 0, value: 'Pylontech' },
                { key: 1, value: 'Pytes' },
                { key: 3, value: 'JkBmsSerial' },
                { key: 4, value: 'DalyBms' },
                { key: 5, value: 'Victron' },
                { key: 6, value: 'Mqtt' },
            ],
            jkBmsInterfaceTypeList: [
                { key: 0, value: 'Uart' },
                { key: 1, value: 'Transceiver' },
            ],
            frequencyTypeList: [
                { key: 8, value: 8000000 },
                { key: 16, value: 16000000 },
                { key: 20, value: 20000000 },
            ],
            voltageUnitTypeList: [
                { key: 3, value: 'mV' },
                { key: 2, value: 'cV' },
                { key: 1, value: 'dV' },
                { key: 0, value: 'V' },
            ],
        };
    },
    created() {
        this.getBatteryConfig();
    },
    methods: {
        getIoProvider() {
            if (((this.batteryConfigList.provider == 1 || this.batteryConfigList.provider == 2) &&
                 (this.batteryConfigList.io_providername == 'RS232' || this.batteryConfigList.io_providername == 'RS485')
                ) ||
                (this.batteryConfigList.provider > 2 && this.batteryConfigList.provider < 5 &&
                 (this.batteryConfigList.io_providername != 'RS232' && this.batteryConfigList.io_providername != 'RS485')
                ) ||
                (this.batteryConfigList.provider == 5 && this.batteryConfigList.io_providername != 'RS232'))
            {
                this.alertMessage = 'I/O interface ' + this.batteryConfigList.io_providername + ' is not supported for this battery provider. Please configure correct interface in pinmapping.json file';
                this.alertType = 'error';
                this.showAlert = true;
            } else {
                this.showAlert = false;
            }

            return this.batteryConfigList.provider < 6 ? this.batteryConfigList.io_providername : 'MQTT';
        },
        getBatteryConfig() {
            this.dataLoading = true;
            fetch('/api/battery/config', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.batteryConfigList = data;
                    this.dataLoading = false;
                });
        },
        saveBatteryConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append('data', JSON.stringify(this.batteryConfigList));

            fetch('/api/battery/config', {
                method: 'POST',
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
