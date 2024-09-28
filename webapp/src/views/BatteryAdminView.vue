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
                    <InputElement :label="$t('batteryadmin.UpdatesOnly')"
                        v-model="batteryConfigList.updatesonly"
                        type="checkbox" wide4_1 />

                    <InputElement :label="$t('batteryadmin.VerboseLogging')"
                        v-model="batteryConfigList.verbose_logging"
                        type="checkbox" wide4_1/>

                    <div class="row mb-3">
                        <label class="col-sm-4 col-form-label">
                            {{ $t('batteryadmin.Provider')}} {{ getIoProvider() }} {{ $t('batteryadmin.Interface')}}
                        </label>
                        <div class="col-sm-5">
                            <select class="form-select" v-model="batteryConfigList.provider">
                                <option v-for="provider in providerTypeList" :key="provider.key" :value="provider.key">
                                    {{ $t(`batteryadmin.Provider` + provider.value) }}
                                </option>
                            </select>
                        </div>
                    </div>

                    <InputElement v-show="batteryConfigList.provider < 3"
                        :label="$t('batteryadmin.NumberOfBatteries')"
                        v-model="batteryConfigList.numberOfBatteries" type="number" min="1" max="5" wide4_1 />
                </div>
            </CardElement>

            <div v-show="batteryConfigList.enabled">
                <CardElement :text="$t('batteryadmin.BatteryParameter')" textVariant="text-bg-primary" add-space>

                    <InputElement v-show="batteryConfigList.provider < 7"
                        :label="$t('batteryadmin.PollInterval')"
                        v-model="batteryConfigList.pollinterval" type="number" min="2" max="90" wide4_2
                        :postfix="$t('batteryadmin.Seconds')" />

                    <InputElement v-if="batteryConfigList.provider >= 7"
                        :label="$t('batteryadmin.RecommendedChargeVoltage')"
                        v-model="batteryConfigList.recommended_charge_voltage" type="number" min="21" max="80"
                        step="0.1" wide4_2 :postfix="$t('batteryadmin.Volts')" />

                    <InputElement v-if="batteryConfigList.provider >= 7"
                        :label="$t('batteryadmin.RecommendedDischargeVoltage')"
                        v-model="batteryConfigList.recommended_discharge_voltage" type="number" min="21" max="80"
                        step="0.1" wide4_2 :postfix="$t('batteryadmin.Volts')" />

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

                    <InputElement v-if="batteryConfigList.provider != 10"
                        :label="$t('batteryadmin.StopChargingSoC')"
                        v-model="batteryConfigList.stop_charging_soc" type="number" step="1" min="20" max="100" wide4_2
                        :tooltip="$t('batteryadmin.StopChargingSoCHint')" :postfix="$t('batteryadmin.Percent')" />
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

                <template v-if="batteryConfigList.mqtt != null && batteryConfigList.enabled && batteryConfigList.provider == 9">
                    <CardElement :text="$t('batteryadmin.MqttSocConfiguration')" textVariant="text-bg-primary" addSpace>

                        <InputElement :label="$t('batteryadmin.MqttSocTopic')"
                            v-model="batteryConfigList.mqtt.soc_topic" type="text" maxlength="256" wide3_9/>

                        <InputElement :label="$t('batteryadmin.MqttJsonPath')"
                            v-model="batteryConfigList.mqtt.soc_json_path" type="text" maxlength="128"
                            :tooltip="$t('batteryadmin.MqttJsonPathDescription')" wide3_9/>

                    </CardElement>

                    <CardElement :text="$t('batteryadmin.MqttVoltageConfiguration')" textVariant="text-bg-primary"
                        addSpace>

                        <InputElement :label="$t('batteryadmin.MqttVoltageTopic')"
                            v-model="batteryConfigList.mqtt.voltage_topic" type="text" maxlength="256" wide3_9/>

                        <InputElement :label="$t('batteryadmin.MqttJsonPath')"
                            v-model="batteryConfigList.mqtt.voltage_json_path" type="text" maxlength="128"
                            :tooltip="$t('batteryadmin.MqttJsonPathDescription')" wide3_9/>

                        <div class="row mb-3">
                            <label for="mqtt_voltage_unit" class="col-sm-3 col-form-label">
                                {{ $t('batteryadmin.MqttVoltageUnit') }}
                            </label>
                            <div class="col-sm-1">
                                <select id="mqtt_voltage_unit" class="form-select"
                                    v-model="batteryConfigList.mqtt.voltage_unit">
                                    <option v-for="u in voltageUnitTypeList" :key="u.key" :value="u.key">
                                        {{ u.value }}
                                    </option>
                                </select>
                            </div>
                        </div>
                    </CardElement>
                </template>

                <template v-if="batteryConfigList.enabled">
                    <CardElement :text="$t('batteryadmin.DischargeCurrentLimitConfiguration')" textVariant="text-bg-primary"
                        addSpace>
                        <InputElement :label="$t('batteryadmin.LimitDischargeCurrent')"
                            v-model="batteryConfigList.enableDischargeCurrentLimit" type="checkbox" wide3_1 />

                        <template v-if="batteryConfigList.enableDischargeCurrentLimit">
                            <InputElement :label="$t('batteryadmin.DischargeCurrentLimit')"
                                v-model="batteryConfigList.dischargeCurrentLimit" type="number" min="0" step="0.1" postfix="A" wide3_2 />

                            <InputElement :label="$t('batteryadmin.UseBatteryReportedDischargeCurrentLimit')"
                                v-model="batteryConfigList.useBatteryReportedDischargeCurrentLimit" type="checkbox" wide3_1 />
                        </template>

                        <template v-if="batteryConfigList.enableDischargeCurrentLimit && batteryConfigList.useBatteryReportedDischargeCurrentLimit">
                            <div class="alert alert-secondary"
                                role="alert"
                                v-html="$t('batteryadmin.UseBatteryReportedDischargeCurrentLimitInfo')">
                            </div>
                            <template v-if="batteryConfigList.mqtt != null &&  batteryConfigList.provider == 9">
                                <InputElement :label="$t('batteryadmin.MqttDischargeCurrentTopic')"
                                    v-model="batteryConfigList.mqtt.discharge_current_topic" type="text" maxlength="256" wide3_9/>

                                <InputElement :label="$t('batteryadmin.MqttJsonPath')"
                                    v-model="batteryConfigList.mqtt.discharge_current_json_path" type="text" maxlength="128"
                                    :tooltip="$t('batteryadmin.MqttJsonPathDescription')" wide3_9/>

                                <div class="row mb-3">
                                    <label for="mqtt_amperage_unit" class="col-sm-3 col-form-label">
                                        {{ $t('batteryadmin.MqttAmperageUnit') }}
                                    </label>
                                    <div class="col-sm-1">
                                        <select id="mqtt_amperage_unit" class="form-select"
                                            v-model="batteryConfigList.mqtt.amperage_unit">
                                            <option v-for="u in amperageUnitTypeList" :key="u.key" :value="u.key">
                                                {{ u.value }}
                                            </option>
                                        </select>
                                    </div>
                                </div>
                            </template>
                        </template>
                    </CardElement>
                </template>

                <template v-if="batteryConfigList.enabled && batteryConfigList.zendure !=null && batteryConfigList.provider == 10">
                    <CardElement :text="$t('batteryadmin.ZendureConfiguration')" textVariant="text-bg-primary" addSpace>
                        <div class="row mb-3">
                            <label for="zendure_device_type" class="col-sm-3 col-form-label">
                                {{ $t('batteryadmin.ZendureDeviceType') }}
                            </label>
                            <div class="col-sm-4">
                                <select
                                    id="zendure_device_type"
                                    class="form-select"
                                    v-model="batteryConfigList.zendure.device_type">
                                    <option v-for="u in zendureDeviceTypeList" :key="u.key" :value="u.key">
                                        {{ u.value }}
                                    </option>
                                </select>
                            </div>
                        </div>

                        <InputElement :label="$t('batteryadmin.ZendureDeviceSerial')"
                            v-model="batteryConfigList.zendure.device_serial"
                            type="text" minlength="8" maxlength="8" wide3_4/>

                        <InputElement :label="$t('batteryadmin.ZendureMaxOutput')"
                            v-model="batteryConfigList.zendure.max_output"
                            type="number" min="100" max="2000" step="100"
                            :postfix="$t('batteryadmin.Watt')" wide3_2/>

                        <InputElement :label="$t('batteryadmin.ZendureMinSoc')"
                            v-model="batteryConfigList.zendure.soc_min"
                            type="number" min="0" max="60" step="1"
                            :postfix="$t('batteryadmin.Percent')" wide3_2/>

                        <InputElement :label="$t('batteryadmin.ZendureMaxSoc')"
                            v-model="batteryConfigList.zendure.soc_max"
                            type="number" min="40" max="100" step="1"
                            :postfix="$t('batteryadmin.Percent')" wide3_2/>

                        <div class="row mb-3">
                            <label for="zendure_bypass_mode" class="col-sm-3 col-form-label">
                                {{ $t('batteryadmin.ZendureBypassMode') }}
                            </label>
                            <div class="col-sm-4">
                                <select
                                    id="zendure_bypass_mode"
                                    class="form-select"
                                   v-model="batteryConfigList.zendure.bypass_mode">
                                    <option v-for="u in zendureBypassModeList" :key="u.key" :value="u.key">
                                        {{ $t(`batteryadmin.ZendureBypassMode` + u.value) }}
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
                { key: 1, value: 'GobelPowerRN' },
                { key: 2, value: 'Pytes' },
                { key: 3, value: 'SBSCan' },
                { key: 4, value: 'JkBmsSerial' },
                { key: 5, value: 'JbdBmsSerial' },
                { key: 6, value: 'DalyBms' },
                { key: 7, value: 'VictronShunt' },
                { key: 8, value: 'VictronSense' },
                { key: 9, value: 'Mqtt' },
                { key: 10, value: 'ZendureLocalMqtt' },
            ],
            zendureDeviceTypeList: [
                { key: 0, value: 'Hub 1200' },
                { key: 1, value: 'Hub 2000' },
                { key: 2, value: 'AIO 2400' },
                { key: 3, value: 'Ace 2000' },
                { key: 4, value: 'Hyper 2000' },
            ],
            zendureBypassModeList: [
                { key: 0, value: 'Automatic' },
                { key: 1, value: 'AlwaysOff' },
                { key: 2, value: 'AlwaysOn' },
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
            amperageUnitTypeList: [
                { key: 1, value: 'mA' },
                { key: 0, value: 'A' },
            ],
        };
    },
    created() {
        this.getBatteryConfig();
    },
    methods: {
        getIoProvider() {
            if ((this.batteryConfigList.provider == 1 && this.batteryConfigList.io_providername != 'RS485') ||
                ((this.batteryConfigList.provider == 2 || this.batteryConfigList.provider == 3) &&
                 (this.batteryConfigList.io_providername == 'RS232' || this.batteryConfigList.io_providername == 'RS485')
                ) ||
                (this.batteryConfigList.provider > 3 && this.batteryConfigList.provider < 7 &&
                 (this.batteryConfigList.io_providername != 'RS232' && this.batteryConfigList.io_providername != 'RS485')
                ) ||
                (this.batteryConfigList.provider == 7 && this.batteryConfigList.io_providername != 'RS232'))
            {
                this.alertMessage = 'I/O interface ' + this.batteryConfigList.io_providername + ' is not supported for this battery provider. Please configure correct interface in pinmapping.json file';
                this.alertType = 'error';
                this.showAlert = true;
            } else {
                this.showAlert = false;
            }

            return this.batteryConfigList.provider < 9 ? this.batteryConfigList.io_providername : 'MQTT';
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
