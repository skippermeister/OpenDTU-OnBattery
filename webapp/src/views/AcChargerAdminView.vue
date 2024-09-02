<template>
    <BasePage :title="$t('acchargeradmin.ChargerSettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <form @submit="saveChargerConfig">
            <CardElement :text="$t('acchargeradmin.ChargerConfiguration')" textVariant="text-bg-primary">
                <InputElement :label="$t('acchargeradmin.Enable') +
                    acChargerConfigList.chargerType +
                    $t('acchargeradmin.on') +
                    acChargerConfigList.io_providername +
                    ' Interface'" v-model="acChargerConfigList.enabled"
                    type="checkbox" wide4_1 />

                <div v-show="acChargerConfigList.enabled">
                    <InputElement :label="$t('acchargeradmin.UpdatesOnly')" v-model="acChargerConfigList.updatesonly"
                        type="checkbox" wide4_1 />

                    <InputElement :label="$t('acchargeradmin.VerboseLogging')"
                        v-model="acChargerConfigList.verbose_logging" type="checkbox" wide4_1 />

                    <div v-if="acChargerConfigList.huawei != null">
                        <InputElement
                            :label="$t('acchargeradmin.EnableAutoPower')"
                            v-model="acChargerConfigList.huawei.auto_power_enabled"
                            type="checkbox"
                            wide4_2/>

                        <InputElement
                            v-show="acChargerConfigList.huawei.auto_power_enabled"
                            :label="$t('acchargeradmin.EnableBatterySoCLimits')"
                            v-model="acChargerConfigList.huawei.auto_power_batterysoc_limits_enabled"
                            type="checkbox"
                            wide4_2/>

                        <InputElement
                            :label="$t('acchargeradmin.EnableEmergencyCharge')"
                            v-model="acChargerConfigList.huawei.emergency_charge_enabled"
                            type="checkbox"
                            wide4_2/>
                    </div>
                </div>
            </CardElement>

            <div v-show="acChargerConfigList.enabled">
                <CardElement
                    v-show="'can_controller_frequency' in acChargerConfigList"
                    :text="$t('acchargeradmin.CanControllerConfiguration')" textVariant="text-bg-primary" addSpace>
                    <div class="row mb-3">
                        <label class="col-sm-4 col-form-label">
                            {{ $t('acchargeradmin.CanControllerFrequency') }}
                        </label>
                        <div class="col-sm-2">
                            <select class="form-select" v-model="acChargerConfigList.can_controller_frequency">
                                <option v-for="frequency in frequencyTypeList" :key="frequency.key"
                                    :value="frequency.value">
                                    {{ frequency.key }} MHz
                                </option>
                            </select>
                        </div>
                    </div>
                </CardElement>

                <CardElement
                    v-if="acChargerConfigList.meanwell != null"
                    :text="$t('acchargeradmin.ChargerParameter')"
                    textVariant="text-bg-primary" add-space>

                    <div class="row mb-3">
                        <label class="col-sm-4 col-form-label">
                            {{ $t('acchargeradmin.EEPROMwrites') }}
                        </label>
                        <label class="col-sm-4">
                            {{ $n(acChargerConfigList.meanwell.EEPROMwrites, 'decimal') }}
                        </label>
                    </div>

                    <InputElement :label="$t('acchargeradmin.mustInverterProduce')"
                        v-model="acChargerConfigList.meanwell.mustInverterProduce"
                        type="checkbox"
                        wide4_1 />

                    <InputElement :label="$t('acchargeradmin.PollInterval')"
                        v-model="acChargerConfigList.meanwell.pollinterval"
                        type="number" min="1" max="300" placeholder="5"
                        wide4_2
                        :postfix="$t('acchargeradmin.Seconds')" />

                    <InputElement :label="$t('acchargeradmin.Min_Voltage')"
                        v-model="acChargerConfigList.meanwell.min_voltage"
                        type="number" step="0.01" min="21.00" max="80.00" placeholder="47,50"
                        wide4_2
                        :postfix="$t('acchargeradmin.Volts')" />

                    <InputElement :label="$t('acchargeradmin.Max_Voltage')"
                        v-model="acChargerConfigList.meanwell.max_voltage"
                        type="number" step="0.01" min="21.00" max="80.00" placeholder="53,25"
                        wide4_2
                        :postfix="$t('acchargeradmin.Volts')" />

                    <InputElement :label="$t('acchargeradmin.Min_Current')"
                        v-model="acChargerConfigList.meanwell.min_current"
                        type="number" step="0.01" min="1.36" max="50.00" placeholder="3,60"
                        wide4_2
                        :postfix="$t('acchargeradmin.Amps')" />

                    <InputElement :label="$t('acchargeradmin.Max_Current')"
                        v-model="acChargerConfigList.meanwell.max_current"
                        type="number" step="0.01" min="1.36" max="50.00" placeholder="18,00"
                        wide4_2
                        :postfix="$t('acchargeradmin.Amps')" />

                    <InputElement :label="$t('acchargeradmin.Hysteresis')"
                        v-model="acChargerConfigList.meanwell.hysteresis"
                        type="number" step="1" min="1" max="250" placeholder="1"
                        wide4_2
                        :postfix="$t('acchargeradmin.Watts')" />
                </CardElement>

                <div v-if="acChargerConfigList.huawei != null">
                <CardElement
                    v-show="acChargerConfigList.huawei.auto_power_enabled ||
                            acChargerConfigList.huawei.emergency_charge_enabled"
                    :text="$t('acchargeradmin.Limits')"
                    textVariant="text-bg-primary"
                    add-space>

                    <div class="row mb-3">
                        <InputElement
                            :label="$t('acchargeradmin.VoltageLimit')"
                            :tooltip="$t('acchargeradmin.stopVoltageLimitHint')"
                            v-model="acChargerConfigList.huawei.voltage_limit"
                            type="number" placeholder="42" step="0.01" min="42" max="58.5"
                            :postfix="$t('acchargeradmin.Volts')"
                            wide4_2 />

                        <InputElement
                            :label="$t('acchargeradmin.enableVoltageLimit')"
                            :tooltip="$t('acchargeradmin.enableVoltageLimitHint')"
                            v-model="acChargerConfigList.huawei.enable_voltage_limit"
                            type="number" placeholder="42" step="0.01" min="42" max="58.5"
                            :postfix="$t('acchargeradmin.Volts')"
                            wide4_2 />

                        <InputElement
                            :label="$t('acchargeradmin.lowerPowerLimit')"
                            v-model="acChargerConfigList.huawei.lower_power_limit"
                            type="number" placeholder="150" min="50" max="3000"
                            :postfix="$t('acchargeradmin.Watts')"
                            wide4_2/>

                        <InputElement
                            :label="$t('acchargeradmin.upperPowerLimit')"
                            :tooltip="$t('acchargeradmin.upperPowerLimitHint')"
                            v-model="acChargerConfigList.huawei.upper_power_limit"
                            type="number" placeholder="2000" min="100" max="3000"
                            :postfix="$t('acchargeradmin.Watts')"
                            wide4_2/>

                        <InputElement
                            :label="$t('acchargeradmin.targetPowerConsumption')"
                            :tooltip="$t('acchargeradmin.targetPowerConsumptionHint')"
                            v-model="acChargerConfigList.huawei.target_power_consumption"
                            type="number" placeholder="0"
                            :postfix="$t('acchargeradmin.Watts')"
                            wide4_2/>
                    </div>
                </CardElement>
                </div>

                <div v-if="acChargerConfigList.huawei != null">
                <CardElement
                    v-show="acChargerConfigList.huawei.auto_power_enabled &&
                            acChargerConfigList.huawei.auto_power_batterysoc_limits_enabled"
                    :text="$t('acchargeradmin.BatterySoCLimits')"
                    textVariant="text-bg-primary"
                    add-space>

                    <InputElement
                        :label="$t('acchargeradmin.StopBatterySoCThreshold')"
                        :tooltip="$t('acchargeradmin.StopBatterySoCThresholdHint')"
                        v-model="acChargerConfigList.huawei.stop_batterysoc_threshold"
                        type="number" placeholder="95" min="2" max="99"
                        :postfix="$t('acchargeradmin.Percent')"
                        wide4_2 />

                </CardElement>
                </div>
            </div>

            <FormFooter @reload="getChargerConfig" />
        </form>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from "@/components/BootstrapAlert.vue";
import CardElement from '@/components/CardElement.vue';
import FormFooter from '@/components/FormFooter.vue';
import InputElement from '@/components/InputElement.vue';
import type { AcChargerConfig } from '@/types/AcChargerConfig';
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent } from 'vue';

export default defineComponent({
    components: {
        BasePage,
        BootstrapAlert,
        CardElement,
        FormFooter,
        InputElement
    },
    data() {
        return {
            dataLoading: true,
            acChargerConfigList: {} as AcChargerConfig,
            alertMessage: '',
            alertType: 'info',
            showAlert: false,
            frequencyTypeList: [
                { key: 8, value: 8000000 },
                { key: 16, value: 16000000 },
                { key: 20, value: 20000000 },
            ],
        };
    },
    created() {
        this.getChargerConfig();
    },
    methods: {
        getChargerConfig() {
            this.dataLoading = true;
            fetch('/api/charger/config', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    console.log(data);
                    this.acChargerConfigList = data;
                    this.dataLoading = false;
                });
        },
        saveChargerConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append('data', JSON.stringify(this.acChargerConfigList));

            fetch('/api/charger/config', {
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
