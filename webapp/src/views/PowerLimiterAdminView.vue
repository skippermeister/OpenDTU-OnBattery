<template>
    <BasePage :title="'Dynamic Power limiter Settings'" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <BootstrapAlert v-model="configAlert" variant="warning">
            {{ $t('powerlimiteradmin.ConfigAlertMessage') }}
        </BootstrapAlert>

        <CardElement :text="$t('powerlimiteradmin.ConfigHints')" textVariant="text-bg-primary" v-if="getConfigHints().length">
            <div class="row">
                <div class="col-sm-12">
                    {{ $t('powerlimiteradmin.ConfigHintsIntro') }}
                    <ul>
                        <li v-for="(hint, idx) in getConfigHints()" :key="idx">
                            <b v-if="hint.severity === 'requirement'">{{ $t('powerlimiteradmin.ConfigHintRequirement') }}:</b>
                            {{ $t('powerlimiteradmin.ConfigHint' + hint.subject) }}
                        </li>
                    </ul>
                </div>
            </div>
        </CardElement>

        <form @submit="savePowerLimiterConfig" v-if="!configAlert">
            <CardElement :text="$t('powerlimiteradmin.General')" textVariant="text-bg-primary">
                <InputElement :label="$t('powerlimiteradmin.Enable')"
                                v-model="powerLimiterConfigList.enabled"
                                type="checkbox" wide4_1/>

                <InputElement v-show="isEnabled()"
                                :label="$t('powermeteradmin.PollInterval')"
                                v-model="powerLimiterConfigList.pollinterval"
                                type="number" min="1" max="60" wide4_2
                                :postfix="$t('powerlimiteradmin.Seconds')"/>

                <InputElement v-show="isEnabled()"
                                :label="$t('powerlimiteradmin.UpdatesOnly')"
                                v-model="powerLimiterConfigList.updatesonly"
                                type="checkbox" wide4_1/>

                <InputElement v-show="isEnabled()"
                                :label="$t('powerlimiteradmin.VerboseLogging')"
                                v-model="powerLimiterConfigList.verbose_logging"
                                type="checkbox" wide4_1/>

                <div class="row mb-3" v-show="isEnabled()">
                    <label for="targetPowerConsumption" class="col-sm-4 col-form-label">{{ $t('powerlimiteradmin.TargetPowerConsumption') }}:
                        <BIconInfoCircle v-tooltip :title="$t('powerlimiteradmin.TargetPowerConsumptionHint')" />
                    </label>
                    <div class="col-sm-2">
                        <div class="input-group">
                            <input type="number" class="form-control" id="targetPowerConsumption"
                                placeholder="75" v-model="powerLimiterConfigList.target_power_consumption"
                                aria-describedby="targetPowerConsumptionDescription" required/>
                                <span class="input-group-text" id="targetPowerConsumptionDescription">W</span>
                        </div>
                    </div>
                </div>

                <div class="row mb-3" v-show="isEnabled()">
                    <label for="targetPowerConsumptionHyteresis" class="col-sm-4 col-form-label">{{ $t('powerlimiteradmin.TargetPowerConsumptionHysteresis') }}:
                        <BIconInfoCircle v-tooltip :title="$t('powerlimiteradmin.TargetPowerConsumptionHysteresisHint')" required/>
                    </label>
                    <div class="col-sm-2">
                        <div class="input-group">
                            <input type="number" class="form-control" id="targetPowerConsumptionHysteresis"
                                placeholder="30" min="0" v-model="powerLimiterConfigList.target_power_consumption_hysteresis"
                                aria-describedby="targetPowerConsumptionHysteresisDescription" />
                                <span class="input-group-text" id="targetPowerConsumptionHysteresisDescription">W</span>
                        </div>
                    </div>
                </div>
            </CardElement>

            <CardElement :text="$t('powerlimiteradmin.InverterSettings')" textVariant="text-bg-primary" add-space
                         v-show="isEnabled()"
            >
                <div class="row mb-3">
                    <label class="col-sm-4 col-form-label">
                        {{ $t('powerlimiteradmin.Inverter') }}
                    </label>

                    <div class="col-sm-8">
                        <select class="form-select" v-model="powerLimiterConfigList.inverter_id">
                            <option disabled value="invalid" v-if="powerLimiterConfigList.inverter_serial == 0">
                                {{ $t('powerlimiteradmin.SelectInverter') }}
                            </option>
                            <option v-for="inv in powerLimiterConfigList.inverters" :key="inv.id" :value="inv.id">
                                ID {{ ('00'+inv.id).slice(-2) }}: {{ inv.name }} ({{ inv.type }}) SN:{{ inv.serial.toString(16) }}
                            </option>
                        </select>
                    </div>
                </div>

                <div class="row mb-3">
                    <label for="inverter_channel_id" class="col-sm-4 col-form-label">
                        {{ $t('powerlimiteradmin.InverterChannelId') }}:
                        <BIconInfoCircle v-tooltip :title="$t('powerlimiteradmin.InverterChannelIdHint')" />
                    </label>
                    <div class="col-sm-2">
                        <select class="form-select" v-model="powerLimiterConfigList.inverter_channel_id">
                            <option v-for="inverterChannel in inverterChannelList" :key="inverterChannel.key" :value="inverterChannel.key">
                                {{ inverterChannel.value }}
                            </option>
                        </select>
                    </div>
                </div>

                <div class="row mb-3">
                    <label for="inputLowerPowerLimit" class="col-sm-4 col-form-label">{{ $t('powerlimiteradmin.LowerPowerLimit') }}</label>
                    <div class="col-sm-2">
                        <div class="input-group">
                            <input type="number" class="form-control" id="inputLowerPowerLimit"
                                placeholder="50" min="10" v-model="powerLimiterConfigList.lower_power_limit"
                                aria-describedby="lowerPowerLimitDescription" required/>
                                <span class="input-group-text" id="lowerPowerLimitDescription">W</span>
                        </div>
                    </div>
                </div>

                <div class="row mb-3">
                    <label for="inputUpperPowerLimit" class="col-sm-4 col-form-label">{{ $t('powerlimiteradmin.UpperPowerLimit') }}</label>
                    <div class="col-sm-2">
                        <div class="input-group">
                            <input type="number" class="form-control" id="inputUpperPowerLimit"
                                placeholder="800" v-model="powerLimiterConfigList.upper_power_limit"
                                aria-describedby="upperPowerLimitDescription" required/>
                            <span class="input-group-text" id="upperPowerLimitDescription">W</span>
                        </div>
                    </div>
                </div>

                <InputElement :label="$t('powerlimiteradmin.InverterIsBehindPowerMeter')"
                               v-model="powerLimiterConfigList.is_inverter_behind_powermeter"
                               type="checkbox" wide4_1/>

            </CardElement>

            <CardElement :text="$t('powerlimiteradmin.SolarPassthrough')" textVariant="text-bg-primary" add-space
                         v-show="isEnabled()" v-if="powerLimiterConfigList.solar_charge_controller_enabled"
            >
                <div class="alert alert-secondary" v-show="isEnabled()" role="alert" v-html="$t('powerlimiteradmin.SolarpassthroughInfo')"></div>

                <InputElement :label="$t('powerlimiteradmin.EnableSolarPassthrough')"
                              v-model="powerLimiterConfigList.solar_passthrough_enabled"
                              type="checkbox" wide4_1/>

                <div class="row mb-3" v-show="powerLimiterConfigList.solar_passthrough_enabled">
                    <label for="battery_drain_strategy" class="col-sm-4 col-form-label">
                        {{ $t('powerlimiteradmin.BatteryDrainStrategy') }}:
                    </label>
                    <div class="col-sm-3">
                        <select class="form-select" v-model="powerLimiterConfigList.battery_drain_strategy">
                            <option v-for="batteryDrainStrategy in batteryDrainStrategyList" :key="batteryDrainStrategy.key" :value="batteryDrainStrategy.key">
                                {{ $t(batteryDrainStrategy.value) }}
                            </option>
                        </select>
                    </div>
                </div>

                <div class="row mb-3" v-show="powerLimiterConfigList.solar_passthrough_enabled">
                    <label for="solarPassthroughLosses" class="col-sm-4 col-form-label">{{ $t('powerlimiteradmin.SolarPassthroughLosses') }}</label>
                    <div class="col-sm-2">
                        <div class="input-group">
                            <input type="number" class="form-control" id="solarPassthroughLosses"
                                placeholder="3" v-model="powerLimiterConfigList.solar_passthrough_losses"
                                aria-describedby="solarPassthroughLossesDescription" min="0" max="10" required/>
                                <span class="input-group-text" id="solarPassthroughLossesDescription">%</span>
                        </div>
                    </div>
                </div>

                <div class="alert alert-secondary" role="alert" v-show="powerLimiterConfigList.solar_passthrough_enabled" v-html="$t('powerlimiteradmin.SolarPassthroughLossesInfo')"></div>
            </CardElement>

            <CardElement :text="$t('powerlimiteradmin.SocThresholds')" textVariant="text-bg-primary" add-space
                         v-show="isEnabled()" v-if="powerLimiterConfigList.battery_enabled"
            >
                <InputElement :label="$t('powerlimiteradmin.IgnoreSoc')"
                               v-model="powerLimiterConfigList.ignore_soc"
                               type="checkbox"/>

                <div class="alert alert-secondary" role="alert" v-html="$t('powerlimiteradmin.BatterySocInfo')" v-show="!powerLimiterConfigList.ignore_soc"></div>

                <div class="row mb-3" v-show="!powerLimiterConfigList.ignore_soc">
                    <label for="batterySocStartThreshold" class="col-sm-4 col-form-label">{{ $t('powerlimiteradmin.StartThreshold') }}:</label>
                    <div class="col-sm-2">
                        <div class="input-group">
                            <input type="number" class="form-control" id="batterySocStartThreshold"
                                placeholder="80" v-model="powerLimiterConfigList.battery_soc_start_threshold"
                                aria-describedby="batterySocStartThresholdDescription" min="0" max="100" required/>
                                <span class="input-group-text" id="batterySocStartThresholdDescription">%</span>
                        </div>
                    </div>
                </div>

                <div class="row mb-3" v-show="!powerLimiterConfigList.ignore_soc">
                    <label for="batterySocStopThreshold" class="col-sm-4 col-form-label">{{ $t('powerlimiteradmin.StopThreshold') }}</label>
                    <div class="col-sm-2">
                        <div class="input-group">
                            <input type="number" class="form-control" id="batterySocStopThreshold"
                                placeholder="20" v-model="powerLimiterConfigList.battery_soc_stop_threshold"
                                aria-describedby="batterySocStopThresholdDescription" min="0" max="100" required/>
                                <span class="input-group-text" id="batterySocStopThresholdDescription">%</span>
                        </div>
                    </div>
                </div>

                <div class="row mb-3" v-show="isSolarPassthroughEnabled() && !powerLimiterConfigList.ignore_soc">
                    <label for="batterySocSolarPassthroughStartThreshold" class="col-sm-4 col-form-label">{{ $t('powerlimiteradmin.BatterySocSolarPassthroughStartThreshold') }}
                      <BIconInfoCircle v-tooltip :title="$t('powerlimiteradmin.BatterySocSolarPassthroughStartThresholdHint')" />
                    </label>
                    <div class="col-sm-2">
                        <div class="input-group">
                            <input type="number" class="form-control" id="batterySocSolarPassthroughStartThreshold"
                                placeholder="20" v-model="powerLimiterConfigList.full_solar_passthrough_soc"
                                aria-describedby="batterySocSolarPassthroughStartThresholdDescription" min="0" max="100" required/>
                                <span class="input-group-text" id="batterySocSolarPassthroughStartThresholdDescription">%</span>
                        </div>
                    </div>
                </div>
            </CardElement>

            <CardElement :text="$t('powerlimiteradmin.VoltageThresholds')" textVariant="text-bg-primary" add-space
                         v-show="isEnabled()"
            >
                <div class="row mb-3">
                    <label for="inputVoltageStartThreshold" class="col-sm-4 col-form-label">{{ $t('powerlimiteradmin.StartThreshold') }}</label>
                    <div class="col-sm-2">
                        <div class="input-group">
                            <input type="number" step="0.01" class="form-control" id="inputVoltageStartThreshold"
                                placeholder="50" v-model="powerLimiterConfigList.voltage_start_threshold"
                                aria-describedby="voltageStartThresholdDescription" required/>
                                <span class="input-group-text" id="voltageStartThresholdDescription">V</span>
                        </div>
                    </div>
                </div>

                <div class="row mb-3">
                    <label for="inputVoltageStopThreshold" class="col-sm-4 col-form-label">{{ $t('powerlimiteradmin.StopThreshold') }}</label>
                    <div class="col-sm-2">
                        <div class="input-group">
                            <input type="number" step="0.01" class="form-control" id="inputVoltageStopThreshold"
                                placeholder="47.50" v-model="powerLimiterConfigList.voltage_stop_threshold"
                                aria-describedby="voltageStopThresholdDescription" required/>
                            <span class="input-group-text" id="voltageStopThresholdDescription">V</span>
                        </div>
                    </div>
                </div>

                <div class="row mb-3" v-show="isSolarPassthroughEnabled()">
                    <label for="inputVoltageSolarPassthroughStartThreshold" class="col-sm-4 col-form-label">{{ $t('powerlimiteradmin.FullSolarPassthroughStartThreshold') }}
                      <BIconInfoCircle v-tooltip :title="$t('powerlimiteradmin.FullSolarPassthroughStartThresholdHint')" />
                    </label>
                    <div class="col-sm-2">
                        <div class="input-group">
                            <input type="number" step="0.01" class="form-control" id="inputVoltageSolarPassthroughStartThreshold"
                                placeholder="49" v-model="powerLimiterConfigList.full_solar_passthrough_start_voltage"
                                aria-describedby="voltageSolarPassthroughStartThresholdDescription" required/>
                            <span class="input-group-text" id="voltageSolarPassthroughStartThresholdDescription">V</span>
                        </div>
                    </div>
                </div>

                <div class="row mb-3"  v-show="isSolarPassthroughEnabled()">
                    <label for="inputVoltageSolarPassthroughStopThreshold" class="col-sm-4 col-form-label">{{ $t('powerlimiteradmin.VoltageSolarPassthroughStopThreshold') }}</label>
                    <div class="col-sm-2">
                        <div class="input-group">
                            <input type="number" step="0.01" class="form-control" id="inputVoltageSolarPassthroughStopThreshold"
                                placeholder="49" v-model="powerLimiterConfigList.full_solar_passthrough_stop_voltage"
                                aria-describedby="voltageSolarPassthroughStopThresholdDescription" required/>
                            <span class="input-group-text" id="voltageSolarPassthroughStopThresholdDescription">V</span>
                        </div>
                    </div>
                </div>

                <div class="row mb-3">
                    <label for="inputVoltageLoadCorrectionFactor" class="col-sm-4 col-form-label">{{ $t('powerlimiteradmin.VoltageLoadCorrectionFactor') }}</label>
                    <div class="col-sm-2">
                        <div class="input-group">
                            <input type="number" step="0.0001" class="form-control" id="inputVoltageLoadCorrectionFactor"
                                placeholder="0.0001" v-model="powerLimiterConfigList.voltage_load_correction_factor"
                                aria-describedby="voltageLoadCorrectionFactorDescription" required/>
                            <span class="input-group-text" id="voltageLoadCorrectionFactorDescription">V</span>
                        </div>
                    </div>
                </div>

                <div class="alert alert-secondary" role="alert" v-html="$t('powerlimiteradmin.VoltageLoadCorrectionInfo')"></div>
            </CardElement>

            <CardElement :text="$t('powerlimiteradmin.InverterRestart')" textVariant="text-bg-primary" add-space v-show="isEnabled()">
                <div class="row mb-3">
                    <label for="inverter_restart_hour" class="col-sm-4 col-form-label">
                        {{ $t('powerlimiteradmin.InverterRestartHour') }}:
                        <BIconInfoCircle v-tooltip :title="$t('powerlimiteradmin.InverterRestartHint')" />
                    </label>
                    <div class="col-sm-2">
                        <select class="form-select" v-model="powerLimiterConfigList.inverter_restart_hour">
                            <option v-for="hour in restartHourList" :key="hour.key" :value="hour.key">
                                {{ hour.value }}
                            </option>
                        </select>
                    </div>
                </div>
            </CardElement>

            <FormFooter @reload="getPowerLimiterConfig"/>
        </form>
    </BasePage>
</template>

<script lang="ts">
import { defineComponent } from 'vue';
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from "@/components/BootstrapAlert.vue";
import FormFooter from '@/components/FormFooter.vue';
import { handleResponse, authHeader } from '@/utils/authentication';
import CardElement from '@/components/CardElement.vue';
import InputElement from '@/components/InputElement.vue';
import { BIconInfoCircle } from 'bootstrap-icons-vue';
import type { PowerLimiterConfig } from "@/types/PowerLimiterConfig";

export default defineComponent({
    components: {
        BasePage,
        BootstrapAlert,
        FormFooter,
        CardElement,
        InputElement,
        BIconInfoCircle,
    },
    data() {
        return {
            dataLoading: true,
            powerLimiterConfigList: {} as PowerLimiterConfig,
            inverterChannelList: [
                { key: 0, value: "CH 0" },
                { key: 1, value: "CH 1" },
                { key: 2, value: "CH 2" },
                { key: 3, value: "CH 3" },
            ],
            batteryDrainStrategyList: [
                { key: 0, value: "powerlimiteradmin.BatteryDrainWhenFull"},
                { key: 1, value: "powerlimiteradmin.BatteryDrainAtNight" },
            ],
            restartHourList: [
                { key: -1, value: "- - - -" },
                { key: 0, value: "0:00" },
                { key: 1, value: "1:00" },
                { key: 2, value: "2:00" },
                { key: 3, value: "3:00" },
                { key: 4, value: "4:00" },
                { key: 5, value: "5:00" },
                { key: 6, value: "6:00" },
                { key: 7, value: "7:00" },
                { key: 8, value: "8:00" },
                { key: 9, value: "9:00" },
                { key: 10, value: "10:00" },
                { key: 11, value: "11:00" },
                { key: 12, value: "12:00" },
                { key: 13, value: "13:00" },
                { key: 14, value: "14:00" },
                { key: 15, value: "15:00" },
                { key: 16, value: "16:00" },
                { key: 17, value: "17:00" },
                { key: 18, value: "18:00" },
                { key: 19, value: "19:00" },
                { key: 20, value: "20:00" },
                { key: 21, value: "21:00" },
                { key: 22, value: "22:00" },
                { key: 23, value: "23:00" },
            ],
            alertMessage: "",
            alertType: "info",
            showAlert: false,
            configAlert: false,
        };
    },
    created() {
        this.getPowerLimiterConfig();
    },
    watch: {
        'powerLimiterConfigList.inverter_serial'(newVal) {
            var cfg = this.powerLimiterConfigList;
            const bySerial = cfg.inverters.some(inv => inv.serial === newVal);
            if (bySerial) { return; }

            // previously selected inverter was deleted or inverter_serial uses
            // an old index-based value to identify the inverter. force the
            // user to select a valid inverter serial.
            cfg.inverter_id = 0;
        }
    },
    methods: {
        getConfigHints() {
            var hints = [];

            if (this.powerLimiterConfigList.power_meter_enabled !== true) {
                hints.push({severity: "requirement", subject: "PowerMeterDisabled"});
                this.configAlert = true;
            }

            if (typeof this.powerLimiterConfigList.inverters === "undefined" || this.powerLimiterConfigList.inverters.length == 0) {
                hints.push({severity: "requirement", subject: "NoInverter"});
                this.configAlert = true;
            }

            return hints;
        },
        isEnabled() {
            return this.powerLimiterConfigList.enabled;
        },
        isSolarPassthroughEnabled() {
            return this.powerLimiterConfigList.charge_controller_enabled
                && this.powerLimiterConfigList.solar_passthrough_enabled;
        },
        getPowerLimiterConfig() {
            this.dataLoading = true;
            fetch("/api/powerlimiter/config", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.powerLimiterConfigList = data;
                    this.dataLoading = false;
                });
        },
        savePowerLimiterConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append("data", JSON.stringify(this.powerLimiterConfigList));

            fetch("/api/powerlimiter/config", {
                method: "POST",
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then(
                    (response) => {
                        this.alertMessage = response.message;
                        this.alertType = response.type;
                        this.showAlert = true;
                    }
                );
        },
    },
});
</script>
