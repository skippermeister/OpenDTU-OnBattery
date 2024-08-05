<template>
    <div class="text-center" v-if="dataLoading">
        <div class="spinner-border" role="status">
            <span class="visually-hidden">Loading...</span>
        </div>
    </div>

    <template v-else>
        <div class="row gy-3 mt-0">
            <div class="tab-content col-sm-12 col-md-12" id="v-pills-tabContent">
                <div class="card">
                    <div class="card-header d-flex justify-content-between align-items-center" :class="{
                        'text-bg-danger': meanwellData.data_age > 20,
                        'text-bg-primary': meanwellData.data_age < 19,
                    }">
                        <div class="p-1 flex-grow-1">
                            <div class="d-flex flex-wrap">
                                <div style="padding-right: 2em;">{{ $t('meanwell.ACcharger') }}: {{
                                    meanwellData.manufacturerModelName }}</div>
                                <div style="padding-right: 2em;">
                                    <template v-if="meanwellData.automatic">{{ $t('meanwell.AutomaticCharging')
                                        }}</template>
                                    <template v-else>{{ $t('meanwell.ManualCharging') }}</template>
                                </div>
                                <div style="padding-right: 2em;">
                                    {{ $t('meanwell.DataAge') }} {{ $t('meanwell.Seconds', {
                                        'val':
                                            meanwellData.data_age
                                    })
                                    }}
                                </div>
                            </div>
                        </div>
                        <div class="btn-toolbar p-2" role="toolbar">

                            <div class="btn-group me-2" role="group">
                                <button :disabled="!isLogged" type="button" class="btn btn-sm btn-danger"
                                    @click="onShowMeanwellLimitSettings()" v-tooltip
                                    :title="$t('meanwell.ShowSetLimit')">
                                    <BIconSpeedometer style="font-size:24px;" />
                                </button>
                            </div>

                            <div class="btn-group me-2" role="group">
                                <button :disabled="!isLogged" type="button" class="btn btn-sm btn-danger"
                                    @click="onShowMeanwellPowerSettings()" v-tooltip
                                    :title="$t('meanwell.TurnOnOffAuto')">
                                    <BIconPower style="font-size:24px;" />
                                </button>
                            </div>

                        </div>
                    </div>

                    <div class="card-body">
                        <div class="row flex-row flex-wrap align-items-start g-3">
                            <div class="col order-0">
                                <div class="card" :class="{ 'border-info': true }">
                                    <div class="card-header bg-info">{{ $t('meanwell.Input') }}</div>
                                    <div class="table-responsive">
                                        <table class="table table-striped table-hover" style="margin: 0">
                                            <tbody>
                                                <tr v-for="(prop, key) in meanwellData.inputValues" v-bind:key="key">
                                                    <th scope="row">{{ $t('meanwell.' + key) }}</th>
                                                    <td style="text-align: right; padding-right: 0;">
                                                        <template v-if="typeof prop === 'string'">
                                                            {{ $t('meanwell.' + prop) }}
                                                        </template>
                                                        <template v-else>
                                                            {{ $n(prop.v, 'decimal', {
                                                                minimumFractionDigits: prop.d,
                                                                maximumFractionDigits: prop.d
                                                            })
                                                            }}
                                                        </template>
                                                    </td>
                                                    <td v-if="typeof prop === 'string'"></td>
                                                    <td v-else>{{ prop.u }}</td>
                                                </tr>

                                                <tr>
                                                    <th scope="row">{{ $t('meanwell.operation') }}</th>
                                                    <td style="text-align: right; padding-right: 0;">
                                                        <span class="badge" :class="{
                                                            'text-bg-danger': !meanwellData.operation,
                                                            'text-bg-success': meanwellData.operation
                                                        }">
                                                            <template v-if="meanwellData.operation">{{ $t('meanwell.on')
                                                                }}</template>
                                                            <template v-else>{{ $t('meanwell.off') }}</template>
                                                        </span>
                                                    </td>
                                                    <td></td>
                                                </tr>

                                                <tr>
                                                    <th scope="row">{{ $t('meanwell.cuve') }}</th>
                                                    <td style="text-align: right; padding-right: 0;">
                                                        <span class="badge" :class="{
                                                            'text-bg-danger': !meanwellData.cuve,
                                                            'text-bg-success': meanwellData.cuve
                                                        }">
                                                            <template v-if="meanwellData.cuve">{{
                                                                $t('meanwell.charger_mode') }}</template>
                                                            <template v-else>{{ $t('meanwell.power_supply_mode')
                                                                }}</template>
                                                        </span>
                                                    </td>
                                                    <td></td>
                                                </tr>

                                                <tr>
                                                    <th scope="row">{{ $t('meanwell.stgs') }}</th>
                                                    <td style="text-align: right; padding-right: 0;">
                                                        <span class="badge" :class="{
                                                            'text-bg-danger': meanwellData.stgs,
                                                            'text-bg-success': !meanwellData.stgs
                                                        }">
                                                            <template v-if="meanwellData.stgs">{{
                                                                $t('meanwell.two_stage_charge') }}</template>
                                                            <template v-else>{{ $t('meanwell.three_stage_charge')
                                                                }}</template>
                                                        </span>
                                                    </td>
                                                    <td></td>
                                                </tr>

                                            </tbody>
                                        </table>
                                    </div>
                                </div>
                            </div>
                            <div class="col order-1">
                                <div class="card" :class="{ 'border-info': false }">
                                    <div class="card-header bg-info">{{ $t('meanwell.Output') }}</div>
                                    <div class="table-responsive">
                                        <table class="table table-striped table-hover" style="margin: 0">
                                            <tbody>
                                                <tr v-for="(prop, key) in meanwellData.outputValues" v-bind:key="key">
                                                    <th scope="row">{{ $t('meanwell.' + key) }}</th>
                                                    <td style="text-align: right; padding-right: 0;">
                                                        <template v-if="typeof prop === 'string'">
                                                            {{ $t('meanwell.' + prop) }}
                                                        </template>
                                                        <template v-else>
                                                            {{ $n(prop.v, 'decimal', {
                                                                minimumFractionDigits: prop.d,
                                                                maximumFractionDigits: prop.d
                                                            })
                                                            }}
                                                        </template>
                                                    </td>
                                                    <td v-if="typeof prop === 'string'"></td>
                                                    <td v-else>{{ prop.u }}</td>
                                                </tr>
                                            </tbody>
                                        </table>
                                    </div>
                                </div>
                            </div>

                        </div>
                    </div>
                </div>
            </div>
        </div>

        <div class="modal" id="meanwellLimitSettingView" ref="meanwellLimitSettingView" tabindex="-1">
            <div class="modal-dialog modal-lg">
                <div class="modal-content">
                    <form @submit="onSubmitLimit">
                        <div class="modal-header">
                            <h5 class="modal-title">{{ $t('meanwell.LimitSettings') }}</h5>
                            <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button>
                        </div>
                        <div class="modal-body">

                            <BootstrapAlert v-model="showAlertLimit" :variant="alertTypeLimit">
                                {{ alertMessageLimit }}
                            </BootstrapAlert>
                            <div class="text-center" v-if="meanwellLimitSettingLoading">
                                <div class="spinner-border" role="status">
                                    <span class="visually-hidden">{{ $t('meanwell.Loading') }}</span>
                                </div>
                            </div>

                            <template v-if="!meanwellLimitSettingLoading">

                                <div class="row mb-3">
                                    <label for="inputCurrentLimit" class="col-sm-3 col-form-label">{{
                                        $t('meanwell.CurrentLimit') }} </label>
                                </div>

                                <div class="row mb-3  align-items-center">
                                    <label for="inputVoltageTargetLimit" class="col-sm-3 col-form-label">{{
                                        $t('meanwell.SetVoltageLimit') }}</label>

                                    <div class="col-sm-1">
                                        <div class="form-switch form-check-inline">
                                            <input class="form-check-input" type="checkbox" id="flexSwitchVoltage"
                                                v-model="targetLimitList.voltageValid">
                                        </div>
                                    </div>

                                    <div class="col-sm-2">
                                        <input type="number" step="0.01" name="inputVoltageTargetLimit"
                                            class="form-control" id="inputVoltageTargetLimit"
                                            :min="targetVoltageLimitMin" :max="targetVoltageLimitMax"
                                            v-model="targetLimitList.voltage" :disabled=!targetLimitList.voltageValid>
                                    </div>
                                </div>

                                <div class="row mb-3 align-items-center">
                                    <label for="inputCurrentTargetLimit" class="col-sm-3 col-form-label">{{
                                        $t('meanwell.SetCurrentLimit') }}</label>

                                    <div class="col-sm-1">
                                        <div class="form-switch form-check-inline">
                                            <input class="form-check-input" type="checkbox" id="flexSwitchCurrentt"
                                                v-model="targetLimitList.currentValid">
                                        </div>
                                    </div>

                                    <div class="col-sm-2">
                                        <input type="number" step="0.01" name="inputCurrentTargetLimit"
                                            class="form-control" id="inputCurrentTargetLimit"
                                            :min="targetCurrentLimitMin" :max="targetCurrentLimitMax"
                                            v-model="targetLimitList.current" :disabled=!targetLimitList.currentValid>
                                    </div>
                                </div>

                                <div class="row mb-3  align-items-center">
                                    <label for="inputCurve_CVLimit" class="col-sm-3 col-form-label">{{
                                        $t('meanwell.SetCurve_CVLimit') }}</label>

                                    <div class="col-sm-1">
                                        <div class="form-switch form-check-inline">
                                            <input class="form-check-input" type="checkbox" id="flexSwitchCurve_CV"
                                                v-model="targetLimitList.curveCVvalid">
                                        </div>
                                    </div>

                                    <div class="col-sm-2">
                                        <input type="number" step="0.01" name="inputCurve_CVLimit" class="form-control"
                                            id="inputCurve_CVLimit" :min="targetVoltageLimitMin"
                                            :max="targetVoltageLimitMax" v-model="targetLimitList.curveCV"
                                            :disabled=!targetLimitList.curveCVvalid>
                                    </div>
                                </div>

                                <div class="row mb-3  align-items-center">
                                    <label for="inputCurve_CCLimit" class="col-sm-3 col-form-label">{{
                                        $t('meanwell.SetCurve_CCLimit') }}</label>

                                    <div class="col-sm-1">
                                        <div class="form-switch form-check-inline">
                                            <input class="form-check-input" type="checkbox" id="flexSwitchCurve_CC"
                                                v-model="targetLimitList.curveCCvalid">
                                        </div>
                                    </div>

                                    <div class="col-sm-2">
                                        <input type="number" step="0.01" name="inputCurve_CCLimit" class="form-control"
                                            id="inputCurve_CCLimit" :min="targetCurrentLimitMin"
                                            :max="targetCurrentLimitMax" v-model="targetLimitList.curveCC"
                                            :disabled=!targetLimitList.curveCCvalid>
                                    </div>
                                </div>

                                <div class="row mb-3  align-items-center">
                                    <label for="inputCurve_FVLimit" class="col-sm-3 col-form-label">{{
                                        $t('meanwell.SetCurve_FVLimit') }}</label>

                                    <div class="col-sm-1">
                                        <div class="form-switch form-check-inline">
                                            <input class="form-check-input" type="checkbox" id="flexSwitchCurve_FV"
                                                v-model="targetLimitList.curveFVvalid">
                                        </div>
                                    </div>

                                    <div class="col-sm-2">
                                        <input type="number" step="0.01" name="inputCurve_CCLimit" class="form-control"
                                            id="inputCurve_FVLimit" :min="targetVoltageLimitMin"
                                            :max="targetVoltageLimitMax" v-model="targetLimitList.curveFV"
                                            :disabled=!targetLimitList.curveFVvalid>
                                    </div>
                                </div>

                                <div class="row mb-3  align-items-center">
                                    <label for="inputCurve_TCLimit" class="col-sm-3 col-form-label">{{
                                        $t('meanwell.SetCurve_TCLimit') }}</label>

                                    <div class="col-sm-1">
                                        <div class="form-switch form-check-inline">
                                            <input class="form-check-input" type="checkbox" id="flexSwitchCurve_TC"
                                                v-model="targetLimitList.curveTCvalid">
                                        </div>
                                    </div>

                                    <div class="col-sm-2">
                                        <input type="number" step="0.01" name="inputCurve_TCLimit" class="form-control"
                                            id="inputCurve_TCLimit" :min="targetCurrentLimitMin"
                                            :max="targetCurrentLimitMax" v-model="targetLimitList.curveTC"
                                            :disabled=!targetLimitList.curveTCvalid>
                                    </div>
                                </div>

                                <div class="row mb-3">
                                    <div class="col-sm-9">
                                        <div v-if="targetLimitList.voltage < targetVoltageLimitMin"
                                            class="alert alert-secondary mt-3" role="alert"
                                            v-html="$t('meanwell.LimitHint')"></div>
                                    </div>
                                </div>
                            </template>
                        </div>

                        <div class="modal-footer">
                            <button type="submit" class="btn btn-danger" @click="onSetMeanwellLimitSettings()">{{
                                $t('meanwell.SetLimit') }}</button>
                            <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">{{
                                $t('meanwell.Close')
                            }}</button>
                        </div>
                    </form>
                </div>
            </div>
        </div>

        <div class="modal" id="meanwellPowerSettingView" ref="meanwellPowerSettingView" tabindex="-1">
            <div class="modal-dialog modal-lg">
                <div class="modal-content">
                    <div class="modal-header">
                        <h5 class="modal-title">{{ $t('meanwell.PowerSettings') }}</h5>
                        <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button>
                    </div>
                    <div class="modal-body">

                        <BootstrapAlert v-model="showAlertPower" :variant="alertTypePower">
                            {{ alertMessagePower }}
                        </BootstrapAlert>
                        <div class="text-center" v-if="meanwellPowerSettingLoading">
                            <div class="spinner-border" role="status">
                                <span class="visually-hidden">{{ $t('meanwell.Loading') }}</span>
                            </div>
                        </div>

                        <template v-if="!meanwellPowerSettingLoading">
                            <div class="row mb-3 align-items-center">
                                <label for="inputLastPowerSet" class="col col-form-label">{{
                                    $t('meanwell.LastPowerSetStatus') }}</label>
                                <div class="col">
                                    <span class="badge" :class="{
                                        'text-bg-danger': successCommandPower == 'Failure',
                                        'text-bg-success': successCommandPower == 'Ok',
                                    }">
                                        {{ $t('meanwell.' + successCommandPower) }}
                                    </span>
                                </div>
                            </div>

                            <div class="d-grid gap-2 col-6 mx-auto">
                                <button type="button" class="btn btn-success" @click="onSetMeanwellPowerSettings(1)">
                                    <BIconToggleOn class="fs-4" />&nbsp;{{ $t('meanwell.TurnOn') }}
                                </button>
                                <button type="button" class="btn btn-danger" @click="onSetMeanwellPowerSettings(0)">
                                    <BIconToggleOff class="fs-4" />&nbsp;{{ $t('meanwell.TurnOff') }}
                                </button>
                                <button type="button" class="btn btn-warning" @click="onSetMeanwellPowerSettings(2)">
                                    <BIconArrowCounterclockwise class="fs-4" />&nbsp;{{ $t('meanwell.AutomaticCharging')
                                    }}
                                </button>
                            </div>
                        </template>

                    </div>
                    <div class="modal-footer">
                        <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">{{ $t('meanwell.Close')
                            }}</button>
                    </div>
                </div>
            </div>
        </div>

    </template>
</template>

<script lang="ts">
import { defineComponent } from 'vue';
import type { MeanWell } from '@/types/MeanWellDataStatus';
import type { MeanWellLimitConfig, MeanWellLimitStatus } from '@/types/MeanWellLimitConfig';
import { authHeader, authUrl, handleResponse, isLoggedIn } from '@/utils/authentication';

import * as bootstrap from 'bootstrap';
import {
    BIconArrowCounterclockwise,
    BIconToggleOff,
    BIconToggleOn,
    BIconPower,
    BIconSpeedometer
} from 'bootstrap-icons-vue';

export default defineComponent({
    components: {
        BIconArrowCounterclockwise,
        BIconToggleOff,
        BIconToggleOn,
        BIconPower,
        BIconSpeedometer
    },
    data() {
        return {
            isLogged: this.isLoggedIn(),

            socket: {} as WebSocket,
            heartInterval: 0,
            dataAgeInterval: 0,
            dataLoading: true,
            meanwellData: {} as MeanWell,
            isFirstFetchAfterConnect: true,
            targetVoltageLimitMin: 21,
            targetVoltageLimitMax: 80,
            targetCurrentLimitMin: 0.0,
            targetCurrentLimitMax: 50,
            targetLimitList: {} as MeanWellLimitConfig,
            currentLimitList: {} as MeanWellLimitStatus,
            targetLimitPersistent: false,
            meanwellLimitSettingView: {} as bootstrap.Modal,
            meanwellLimitSettingLoading: true,

            alertMessageLimit: "",
            alertTypeLimit: "info",
            showAlertLimit: false,
            checked: false,

            meanwellPowerSettingView: {} as bootstrap.Modal,
            meanwellPowerSettingLoading: true,
            alertMessagePower: "",
            alertTypePower: "info",
            showAlertPower: false,
            successCommandPower: ""
        };
    },
    created() {
        this.getInitialData();
        this.initSocket();
        this.initDataAgeing();
        this.$emitter.on("logged-in", () => {
            this.isLogged = this.isLoggedIn();
        });
        this.$emitter.on("logged-out", () => {
            this.isLogged = this.isLoggedIn();
        });
    },
    unmounted() {
        this.closeSocket();
    },
    methods: {
        isLoggedIn,
        getInitialData() {
            console.log("Get initalData for MeanWell");
            this.dataLoading = true;

            fetch("/api/meanwelllivedata/status", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.meanwellData = data;
                    this.dataLoading = false;
                });
        },
        initSocket() {
            console.log("Starting connection to MeanWell WebSocket Server");

            const { protocol, host } = location;
            const authString = authUrl();
            const webSocketUrl = `${protocol === "https:" ? "wss" : "ws"
                }://${authString}${host}/meanwelllivedata`;

            this.socket = new WebSocket(webSocketUrl);

            this.socket.onmessage = (event) => {
                console.log(event);
                this.meanwellData = JSON.parse(event.data);
                this.dataLoading = false;
                this.heartCheck(); // Reset heartbeat detection
            };

            this.socket.onopen = function (event) {
                console.log(event);
                console.log("Successfully connected to the MeanWell websocket server...");
            };

            // Listen to window events , When the window closes , Take the initiative to disconnect websocket Connect
            window.onbeforeunload = () => {
                this.closeSocket();
            };
        },
        initDataAgeing() {
            this.dataAgeInterval = setInterval(() => {
                if (this.meanwellData) {
                    this.meanwellData.data_age++;
                }
            }, 1000);
        },
        // Send heartbeat packets regularly * 59s Send a heartbeat
        heartCheck() {
            this.heartInterval && clearTimeout(this.heartInterval);
            this.heartInterval = setInterval(() => {
                if (this.socket.readyState === 1) {
                    // Connection status
                    this.socket.send("ping");
                } else {
                    this.initSocket(); // Breakpoint reconnection 5 Time
                }
            }, 59 * 1000);
        },
        /** To break off websocket Connect */
        closeSocket() {
            this.socket.close();
            this.heartInterval && clearTimeout(this.heartInterval);
            this.isFirstFetchAfterConnect = true;
        },
        formatNumber(num: number) {
            return new Intl.NumberFormat(
                undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 }
            ).format(num);
        },
        onHideMeanwellLimitSettings() {
            this.showAlertLimit = false;
        },
        onShowMeanwellLimitSettings() {
            this.meanwellLimitSettingView = new bootstrap.Modal('#meanwellLimitSettingView');
            this.meanwellLimitSettingLoading = true;
            fetch("/api/meanwell/limit/config", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    console.log(data);
                    this.targetLimitList.voltage = data.voltage;
                    this.targetLimitList.current = data.current;
                    this.targetLimitList.curveCV = data.curveCV;
                    this.targetLimitList.curveCC = data.curveCC;
                    this.targetLimitList.curveFV = data.curveFV;
                    this.targetLimitList.curveTC = data.curveTC;
                    this.meanwellLimitSettingLoading = false;
                });
            this.meanwellLimitSettingView.show();
        },
        onSetMeanwellLimitSettings() {
        },
        onSubmitLimit(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append("data", JSON.stringify(this.targetLimitList));

            console.log(this.targetLimitList);

            fetch("/api/meanwell/limit/config", {
                method: "POST",
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then(
                    (response) => {
                        if (response.type == "success") {
                            this.meanwellLimitSettingView.hide();
                        } else {
                            this.alertMessageLimit = this.$t('apiresponse.' + response.code, response.param);
                            this.alertTypeLimit = response.type;
                            this.showAlertLimit = true;
                        }
                    }
                )
        },

        onShowMeanwellPowerSettings() {
            this.meanwellPowerSettingView = new bootstrap.Modal('#meanwellPowerSettingView');
            this.meanwellPowerSettingLoading = true;
            fetch("/api/meanwell/power/config", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    console.log(data);
                    this.successCommandPower = data.power_set_status;
                    this.meanwellPowerSettingLoading = false;
                });
            this.meanwellPowerSettingView.show();
        },

        onHideMeanwellPowerSettings() {
            this.showAlertPower = false;
        },

        onSetMeanwellPowerSettings(power_set: number) {
            let data = {};
            data = {
                power: power_set
            };

            const formData = new FormData();
            formData.append("data", JSON.stringify(data));

            console.log(data);

            fetch("/api/meanwell/power/config", {
                method: "POST",
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then(
                    (response) => {
                        if (response.type == "success") {
                            this.meanwellPowerSettingView.hide();
                        } else {
                            this.alertMessagePower = this.$t('apiresponse.' + response.code, response.param);
                            this.alertTypePower = response.type;
                            this.showAlertPower = true;
                        }
                    }
                )
        },
    },
});
</script>
