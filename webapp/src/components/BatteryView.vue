<template>
    <div class="text-center" v-if="dataLoading">
        <div class="spinner-border" role="status">
            <span class="visually-hidden">Loading...</span>
        </div>
    </div>

    <template v-else>
        <div class="row gy-3 mt-0">
            <div class="col-sm-3 col-md-2" :style="[packData.length == 1 ? { 'display': 'none' } : {}]">
                <div class="nav nav-pills row-cols-sm-1" id="v-pills-tab-pack" role="tablist" aria-orientation="vertical">
                    <button v-for="pack in packData" :key="pack.moduleNumber" class="nav-link border border-primary text-break"
                        :id="'v-pills-' + pack.moduleNumber + '-tab'" data-bs-toggle="pill"
                        :data-bs-target="'#v-pills-' + pack.moduleNumber" type="button" role="tab"
                        aria-controls="'v-pills-' + pack.moduleNumber" aria-selected="true">
                        <div class="row">
                            <div class="col-auto col-sm-6">
                                {{ pack.moduleName }}
                            </div>
                            <div class="col-sm-5">
                                {{ pack.moduleRole }}
                            </div>
                        </div>
                    </button>
                </div>
            </div>

            <div class="tab-content" id="v-pills-tabContent" :class="{
                'col-sm-9 col-md-10': packData.length > 1,
                'col-sm-12 col-md-12': packData.length == 1
            }">
                <div v-for="pack in packData" :key="pack.moduleNumber" class="tab-pane fade show"
                    :id="'v-pills-' + pack.moduleNumber" role="tabpanel"
                    :aria-labelledby="'v-pills-' + pack.moduleNumber + '-tab'" tabindex="0">

                    <div class="card">
                        <div class="card-header d-flex justify-content-between align-items-center"
                            :class="{
                                'text-bg-danger': batteryData.data_age > 20,
                                'text-bg-primary': batteryData.data_age < 20,
                            }">
                            <div class="p-1 flex-grow-1">
                                <div class="d-flex flex-wrap">
                                    <div style="padding-right: 2em;">
                                        {{ $t('battery.battery') }}: {{ batteryData.manufacturer }}
                                    </div>
                                    <div style="padding-right: 2em;" v-if="'numberOfPacks' in batteryData && batteryData.numberOfPacks <= 1">
                                        {{ pack.moduleRole }} {{ pack.moduleName }}
                                    </div>
                                    <div style="padding-right: 2em;" v-if="'fwversion' in batteryData">
                                      {{ $t('battery.FwVersion') }}: {{ batteryData.fwversion }}
                                    </div>
                                    <div style="padding-right: 2em;" v-if="'hwversion' in batteryData">
                                      {{ $t('battery.HwVersion') }}: {{ batteryData.hwversion }}
                                    </div>
                                    <div style="padding-right: 2em;" v-if="'swversion' in batteryData">
                                      {{ $t('battery.SwVersion') }}: {{ batteryData.swversion }}
                                    </div>
                                    <div style="padding-right: 2em;"
                                        v-if="'numberOfPacks' in batteryData && 'swversion' in pack &&
                                              batteryData.numberOfPacks <= 1">
                                        {{ $t('battery.SwVersion') }}: {{ pack.swversion }}
                                    </div>
                                    <div style="padding-right: 2em;" v-if="'moduleSerialNumber' in batteryData">
                                        S/N: {{ batteryData.moduleSerialNumber }}
                                    </div>
                                    <div style="padding-right: 2em;"
                                        v-if="'numberOfPacks' in batteryData && 'moduleSerialNumber' in pack &&
                                              batteryData.numberOfPacks <= 1">
                                        S/N: {{ pack.moduleSerialNumber }}
                                    </div>
                                    <div style="padding-right: 2em;">
                                        {{ $t('battery.DataAge') }} {{ $t('battery.Seconds', { 'val': batteryData.data_age }) }}
                                    </div>

                                </div>
                            </div>
                        </div>

                        <div class="card-body">
                            <div class="row flex-row flex-wrap align-items-start g-3">

                                <div class="col order-0">
                                    <div class="card" :class="{ 'border-info': true }" style="overflow: hidden">
                                        <div class="card-header bg-info">{{ $t('battery.Status') }}</div>
                                        <div class="table-responsive">
                                            <table class="table table-striped table-hover" style="margin: 0">
                                                <tbody>
                                                    <tr v-for="(prop, key) in batteryData.values" v-bind:key="key">
                                                        <th scope="row">{{ $t('battery.' + key) }}</th>
                                                        <td style="text-align: right; padding-right: 0;">
                                                            <template v-if="isStringValue(prop) && prop.translate">
                                                                {{ $t('battery.' + prop.value) }}
                                                            </template>
                                                            <template v-else-if="isStringValue(prop)">
                                                                {{ prop.value }}
                                                            </template>
                                                            <template v-else>
                                                                {{ $n(prop.v, 'decimal', {
                                                                    minimumFractionDigits: prop.d,
                                                                    maximumFractionDigits: prop.d})
                                                                }}
                                                            </template>
                                                        </td>
                                                        <td>
                                                            <template v-if="!isStringValue(prop)">
                                                                {{prop.u}}
                                                            </template>
                                                        </td>
                                                    </tr>
                                                </tbody>
                                            </table>
                                        </div>
                                    </div>
                                </div>

                                <div class="col order-1"  v-if="batteryData.issues != null">
                                    <div class="card" style="overflow: hidden">
                                        <div :class="{'card-header': true, 'border-bottom-0': maxIssueValue === 0}">
                                            <div class="d-flex flex-row justify-content-between align-items-baseline">
                                                {{ $t('battery.issues') }}
                                                <div v-if="maxIssueValue === 0" class="badge text-bg-success">{{ $t('battery.noIssues') }}</div>
                                                <div v-else-if="maxIssueValue === 1" class="badge text-bg-warning text-dark">{{ $t('battery.warning') }}</div>
                                                <div v-else-if="maxIssueValue === 2" class="badge text-bg-danger">{{ $t('battery.alarm') }}</div>
                                            </div>
                                        </div>
                                        <div class="table-responsive" v-if="'issues' in batteryData">
                                            <table class="table table-striped table-hover" style="margin: 0">
                                                <tbody>
                                                    <tr v-for="(prop, key) in batteryData.issues" v-bind:key="key">
                                                        <th scope="row">{{ $t('battery.' + key) }}</th>
                                                        <td>
                                                            <span class="badge" :class="{
                                                                'text-bg-warning text-dark': prop === 1,
                                                                'text-bg-danger': prop === 2
                                                                }">
                                                                <template v-if="prop === 1">{{ $t('battery.warning') }}</template>
                                                                <template v-else>{{ $t('battery.alarm') }}</template>
                                                            </span>
                                                        </td>
                                                    </tr>
                                                </tbody>
                                            </table>
                                        </div>
                                    </div>
                                </div>

                                <div class="col order-2">
                                <div class="card">
                                <div class="card-body">
                                <div class="row flex-row flex-wrap align-items-start g-3">

                                <div class="col order-0" v-if="'numberOfPacks' in batteryData && batteryData.numberOfPacks > 1">
                                    <div class="card" :class="{ 'border-info': true }" style="overflow: hidden">
                                        <div class="card-header bg-info">
                                            {{ pack.moduleRole }} {{ pack.moduleName }} (S/N: {{ pack.moduleSerialNumber }}) Software Version: {{ pack.swversion }}
                                        </div>
                                        <div class="table-responsive">
                                            <table class="table table-striped table-hover" style="margin: 0">
                                                <tbody>
                                                    <tr v-for="(prop, key) in pack.values" v-bind:key="key">
                                                        <th scope="row">{{ $t('battery.' + key) }}</th>
                                                        <td style="text-align: right; padding-right: 0;">
                                                            <template v-if="isStringValue(prop) && prop.translate">
                                                                {{ $t('battery.' + prop.value) }}
                                                            </template>
                                                            <template v-else-if="isStringValue(prop)">
                                                                {{ prop.value }}
                                                            </template>
                                                            <template v-else>
                                                                {{ $n(prop.v, 'decimal', {
                                                                    minimumFractionDigits: prop.d,
                                                                    maximumFractionDigits: prop.d})
                                                                }}
                                                            </template>
                                                        </td>
                                                        <td>
                                                            <template v-if="!isStringValue(prop)">
                                                                {{prop.u}}
                                                            </template>
                                                        </td>
                                                    </tr>
                                                </tbody>
                                            </table>
                                        </div>
                                    </div>
                                </div>

                                <div class="col order-1" v-if="pack.cell != null">
                                    <div class="card" :class="{ 'border-info': false }" style="overflow: hidden">
                                        <div class="card-header bg-info">{{ $t('battery.cell_voltages') }}</div>
                                        <div class="table-responsive">
                                            <table class="table table-striped table-hover" style="margin: 0">
                                                <tbody>
                                                    <tr>
                                                        <th scope="row">{{  $t('battery.cellMinVoltage') }}</th>
                                                        <td style="text-align: right; padding-right: 0;">
                                                            {{ $n(pack.cell.cellMinVoltage.v, 'decimal', {
                                                                minimumFractionDigits: pack.cell.cellMinVoltage.d,
                                                                maximumFractionDigits: pack.cell.cellMinVoltage.d})
                                                            }}
                                                        </td>
                                                        <td>{{ pack.cell.cellMinVoltage.u }}</td>

                                                        <th scope="row">{{  $t('battery.cellMaxVoltage') }}</th>
                                                        <td style="text-align: right; padding-right: 0;">
                                                            {{ $n(pack.cell.cellMaxVoltage.v, 'decimal', {
                                                                minimumFractionDigits: pack.cell.cellMaxVoltage.d,
                                                                maximumFractionDigits: pack.cell.cellMaxVoltage.d})
                                                            }}
                                                        </td>
                                                        <td>{{ pack.cell.cellMaxVoltage.u }}</td>

                                                        <th scope="row">{{  $t('battery.cellDiffVoltage') }}</th>
                                                        <td style="text-align: right; padding-right: 0;">
                                                            {{ $n(pack.cell.cellDiffVoltage.v, 'decimal', {
                                                                minimumFractionDigits: pack.cell.cellDiffVoltage.d,
                                                                maximumFractionDigits: pack.cell.cellDiffVoltage.d})
                                                            }}
                                                        </td>
                                                        <td>{{ pack.cell.cellDiffVoltage.u }}</td>
                                                    </tr>

                                                    <template v-if="pack.cell.voltage != null">
                                                        <template v-for="i in Math.floor((pack.cell.voltage.length+2) / 3)" v-bind:key="i">
                                                        <tr>
                                                            <template v-for="(voltage, index) in pack.cell.voltage.slice((i-1)*3,(i-1)*3+3)" v-bind:key="index">
                                                                <th scope="row">{{ $t('battery.cell') }} {{(i-1)*3+index+1}}</th>
                                                                <td style="text-align: right; padding-right: 0;">
                                                                    {{ $n(voltage.v, 'decimal', {
                                                                        minimumFractionDigits: voltage.d,
                                                                        maximumFractionDigits: voltage.d})
                                                                    }}
                                                                </td>
                                                                <td>{{ voltage.u }}</td>
                                                            </template>
                                                        </tr>
                                                        </template>
                                                    </template>
                                                </tbody>
                                            </table>
                                        </div>

                                        <template v-if="pack.tempSensor != null" >
                                        <div class="card-header bg-info">{{ $t('battery.cell_temperatures') }}</div>
                                        <div class="table-responsive">
                                            <table class="table table-striped table-hover" style="margin: 0">
                                                <tbody>
                                                    <template v-for="i in Math.floor((pack.tempSensor.length+2) / 3)" v-bind:key="i">
                                                        <tr>
                                                            <template v-for="(tempSensor, index) in pack.tempSensor.slice((i-1)*3,(i-1)*3+3)" v-bind:key="index">
                                                                <th scope="row">{{ $t('battery.tempSensor') }} {{(i-1)*3+index+1}}</th>
                                                                <td style="text-align: right; padding-right: 0;">
                                                                    {{ $n(tempSensor.v, 'decimal', {
                                                                        minimumFractionDigits: tempSensor.d,
                                                                        maximumFractionDigits: tempSensor.d})
                                                                    }}
                                                                </td>
                                                                <td>{{ tempSensor.u }}</td>
                                                            </template>
                                                        </tr>
                                                    </template>
                                                </tbody>
                                            </table>
                                        </div>
                                        </template>

                                        <template v-if="pack.parameters != null">
                                        <div class="card-header bg-info">{{ $t('battery.Parameters') }}</div>
                                        <div class="table-responsive">
                                            <table class="table table-striped table-hover" style="margin: 0">
                                                <tbody>
                                                    <tr v-for="(prop, key) in pack.parameters" v-bind:key="key">
                                                        <th scope="row">{{ $t('battery.' + key) }}</th>
                                                        <td style="text-align: right; padding-right: 0;">
                                                            <template v-if="isStringValue(prop) && prop.translate">
                                                                {{ $t('battery.' + prop.value) }}
                                                            </template>
                                                            <template v-else-if="isStringValue(prop)">
                                                                {{ prop.value }}
                                                            </template>
                                                            <template v-else>
                                                                {{ $n(prop.v, 'decimal', {
                                                                    minimumFractionDigits: prop.d,
                                                                    maximumFractionDigits: prop.d})
                                                                }}
                                                            </template>
                                                        </td>
                                                        <td>
                                                            <template v-if="!isStringValue(prop)">
                                                                {{prop.u}}
                                                            </template>
                                                        </td>
                                                    </tr>
                                                </tbody>
                                            </table>
                                        </div>
                                        </template>

                                    </div>
                                </div>
                                </div>
                                </div>
                                </div>
                                </div>

                            </div>
                        </div>

                    </div>
                </div>
            </div>
        </div>
    </template>
</template>

<script lang="ts">
import { defineComponent } from 'vue';
import type { Pack, BatteryData, StringValue } from '@/types/BatteryDataStatus';
import type { ValueObject } from '@/types/LiveDataStatus';
import { handleResponse, authHeader, authUrl } from '@/utils/authentication';
import * as bootstrap from 'bootstrap';

export default defineComponent({
  components: {
  },
  data() {
    return {
      socket: {} as WebSocket,
      heartInterval: 0,
      dataAgeInterval: 0,
      dataLoading: true,
      batteryData: {} as BatteryData,
      isFirstFetchAfterConnect: true,

      alertMessageLimit: "",
      alertTypeLimit: "info",
      showAlertLimit: false,
      checked: false,
    };
  },
  created() {
    this.getInitialData();
    this.initSocket();
    this.initDataAgeing();
  },
  unmounted() {
    this.closeSocket();
  },
  updated() {
    console.log("Battery Updated");
    // Select first tab
    if (this.isFirstFetchAfterConnect) {
        console.log("Battery isFirstFetchAfterConnect");

        this.$nextTick(() => {
            console.log("Battery nextTick");
            const firstTabEl = document.querySelector(
                "#v-pills-tab-pack:first-child button"
            );
            if (firstTabEl != null) {
                this.isFirstFetchAfterConnect = false;
                console.log("Battery Show");
                const firstTab = new bootstrap.Tab(firstTabEl);
                firstTab.show();
            }
        });
    }
  },
  computed: {
    maxIssueValue() {
        return ('issues' in this.batteryData)?Math.max(...Object.values(this.batteryData.issues)):0;
    },
    packData(): Pack[] {
            return this.batteryData.packs.slice().sort((a: Pack, b: Pack) => {
                return a.moduleNumber - b.moduleNumber;
            });
        }
  },
  methods: {
    isStringValue(value: ValueObject | StringValue) : value is StringValue {
      return value && typeof value === 'object' && 'translate' in value;
    },
    getInitialData() {
      console.log("Get initalData for Battery");
      this.dataLoading = true;

      fetch("/api/batterylivedata/status", { headers: authHeader() })
        .then((response) => handleResponse(response, this.$emitter, this.$router))
        .then((data) => {
          console.log(data);
          this.batteryData = data;
          this.dataLoading = false;
        });
    },
    reloadData() {
        this.closeSocket();

        setTimeout(() => {
            this.getInitialData();
            this.initSocket();
        }, 1000);
    },
    initSocket() {
      console.log("Starting connection to Battery WebSocket Server");

      const { protocol, host } = location;
      const authString = authUrl();
      const webSocketUrl = `${protocol === "https:" ? "wss" : "ws"
        }://${authString}${host}/batterylivedata`;

      this.socket = new WebSocket(webSocketUrl);

      this.socket.onmessage = (event) => {
        console.log(event);
        if (event.data != "{}") {
            const newData = JSON.parse(event.data);
            this.batteryData.manufacturer = newData.manufacturer;
            this.batteryData.data_age = newData.data_age;
            this.batteryData.values = newData.values;
            this.batteryData.issues = newData.issues;

            const foundIdx = this.batteryData.packs.findIndex((element) => element.moduleNumber == newData.packs[0].moduleNumber);
            if (foundIdx == -1) {
                Object.assign(this.batteryData.packs, newData.packs);
            } else {
                Object.assign(this.batteryData.packs[foundIdx], newData.packs[0]);
            }
            this.dataLoading = false;
            this.heartCheck(); // Reset heartbeat detection
        } else {
            // Sometimes it does not recover automatically so have to force a reconnect
            this.closeSocket();
            this.heartCheck(10); // Reconnect faster
        }
      };

      this.socket.onopen = function (event) {
        console.log(event);
        console.log("Successfully connected to the Battery websocket server...");
      };

      // Listen to window events , When the window closes , Take the initiative to disconnect websocket Connect
      window.onbeforeunload = () => {
        this.closeSocket();
      };
    },
    initDataAgeing() {
      this.dataAgeInterval = setInterval(()  => {
        if (this.batteryData) {
          this.batteryData.data_age++;
        }
      }, 1000);
    },
    // Send heartbeat packets regularly * 59s Send a heartbeat
    heartCheck(duration: number = 59) {
      this.heartInterval && clearTimeout(this.heartInterval);
      this.heartInterval = setInterval(() => {
        if (this.socket.readyState === 1) {
          // Connection status
          this.socket.send("ping");
        } else {
          this.initSocket(); // Breakpoint reconnection 5 Time
        }
      }, duration * 1000);
    },
    /** To break off websocket Connect */
    closeSocket() {
      this.socket.close();
      this.heartInterval && clearTimeout(this.heartInterval);
      this.isFirstFetchAfterConnect = true;
    }
  },
});
</script>
