<template>

    <div class="text-center" v-if="dataLoading">
        <div class="spinner-border" role="status">
            <span class="visually-hidden">Loading...</span>
        </div>
    </div>

    <template v-else>
        <div class="row gy-3">
            <div class="tab-content col-sm-12 col-md-12" id="v-pills-tabContent">
                <div class="card">
                    <div class="card-header d-flex justify-content-between align-items-center"
                        :class="{
                            'text-bg-danger': refusolData.age_critical,
                            'text-bg-primary': !refusolData.age_critical,
                        }">
                        <div class="p-1 flex-grow-1">
                            <div class="d-flex flex-wrap">
                                <div style="padding-right: 2em;">
                                    {{ refusolData.PID }}
                                </div>
                                <div style="padding-right: 2em;">
                                    {{ $t('refusolhome.SerialNumber') }} {{ refusolData.serNo }}
                                </div>
                                <div style="padding-right: 2em;">
                                    {{ $t('refusolhome.FirmwareNumber') }}  {{ refusolData.firmware }}
                                </div>
                                <div style="padding-right: 2em;">
                                    {{ $t('refusolhome.DataAge') }} {{ $t('refusolhome.Seconds', {'val': refusolData.data_age }) }}
                                </div>
                            </div>
                        </div>
                    </div>
                    <div class="card-body">
                        <div class="row flex-row flex-wrap align-items-start g-3">
                            <div class="col order-0">
                                <div class="card" :class="{ 'border-info': true }">
                                    <div class="card-header text-bg-info">{{ $t('refusolhome.DeviceInfo') }}</div>
                                    <div class="card-body">
                                        <div class="table-responsive">
                                            <table class="table table-striped table-hover" style="margin: 0">
                                                <tbody>
                                                    <tr v-for="(prop, key) in refusolData.deviceValues" v-bind:key="key">
                                                        <th scope="row">{{ $t('refusolhome.' + key) }}</th>
                                                        <td style="text-align: right; padding-right: 0;">
                                                            <template v-if="typeof prop === 'string'">
                                                                {{ $t('refusolhome.' + prop) }}
                                                            </template>
                                                            <template v-else>
                                                                {{ $n(prop.v, 'decimal', {
                                                                    minimumFractionDigits: prop.d,
                                                                    maximumFractionDigits: prop.d})
                                                                }}
                                                            </template>
                                                        </td>
                                                        <td v-if="typeof prop === 'string'"></td>
                                                        <td v-else>{{prop.u}}</td>
                                                    </tr>
                                                </tbody>
                                            </table>
                                        </div>
                                    </div>
                                </div>
                            </div>
                            <div class="col order-1">
                                <div class="card" :class="{ 'border-info': false }">
                                    <div class="card-header">{{ $t('refusolhome.AC') }}</div>
                                    <div class="card-body">
                                        <div class="table-responsive">
                                            <table class="table table-striped table-hover" style="margin: 0">
                                                <tbody>
                                                    <tr v-for="(prop, key) in refusolData.acValues" v-bind:key="key">
                                                        <th scope="row">{{ $t('refusolhome.' + key) }}</th>
                                                        <td style="text-align: right; padding-right: 0;">
                                                            <template v-if="typeof prop === 'string'">
                                                                {{ $t('refusolhome.' + prop) }}
                                                            </template>
                                                            <template v-else>
                                                                {{ $n(prop.v, 'decimal', {
                                                                    minimumFractionDigits: prop.d,
                                                                    maximumFractionDigits: prop.d})
                                                                }}
                                                            </template>
                                                        </td>
                                                        <td v-if="typeof prop === 'string'"></td>
                                                        <td v-else>{{prop.u}}</td>
                                                    </tr>
                                                </tbody>
                                            </table>
                                        </div>
                                    </div>
                                </div>
                            </div>
                            <div class="col order-2">
                                <div class="card" :class="{ 'border-info': false }">
                                    <div class="card-header">{{ $t('refusolhome.Delivered') }}</div>
                                    <div class="card-body">
                                        <div class="table-responsive">
                                            <table class="table table-striped table-hover" style="margin: 0">
                                                <tbody>
                                                    <tr v-for="(prop, key) in refusolData.yieldValues" v-bind:key="key">
                                                        <th scope="row">{{ $t('refusolhome.' + key) }}</th>
                                                        <td style="text-align: right; padding-right: 0;">
                                                            <template v-if="typeof prop === 'string'">
                                                                {{ $t('refusolhome.' + prop) }}
                                                            </template>
                                                            <template v-else>
                                                                {{ $n(prop.v, 'decimal', {
                                                                    minimumFractionDigits: prop.d,
                                                                    maximumFractionDigits: prop.d})
                                                                }}
                                                            </template>
                                                        </td>
                                                        <td v-if="typeof prop === 'string'"></td>
                                                        <td v-else>{{prop.u}}</td>
                                                    </tr>
                                                </tbody>
                                            </table>
                                        </div>
                                    </div>
                                </div>
                            </div>
                            <div class="col order-3">
                                <div class="card" :class="{ 'border-info': false }">
                                    <div class="card-header">{{ $t('refusolhome.Panel') }}</div>
                                    <div class="card-body">
                                        <div class="table-responsive">
                                            <table class="table table-striped table-hover" style="margin: 0">
                                                <tbody>
                                                    <tr v-for="(prop, key) in refusolData.dcValues" v-bind:key="key">
                                                        <th scope="row">{{ $t('refusolhome.' + key) }}</th>
                                                        <td style="text-align: right; padding-right: 0;">
                                                            <template v-if="typeof prop === 'string'">
                                                                {{ $t('refusolhome.' + prop) }}
                                                            </template>
                                                            <template v-else>
                                                                {{ $n(prop.v, 'decimal', {
                                                                    minimumFractionDigits: prop.d,
                                                                    maximumFractionDigits: prop.d})
                                                                }}
                                                            </template>
                                                        </td>
                                                        <td v-if="typeof prop === 'string'"></td>
                                                        <td v-else>{{prop.u}}</td>
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
        </div>
    </template>
</template>

<script lang="ts">
import { defineComponent } from 'vue';
import type { REFUsol } from '@/types/REFUsolLiveDataStatus';
import { handleResponse, authHeader, authUrl } from '@/utils/authentication';

export default defineComponent({
    data() {
        return {
            socket: {} as WebSocket,
            heartInterval: 0,
            dataAgeInterval: 0,
            dataLoading: true,
            refusolData: {} as REFUsol,
            isFirstFetchAfterConnect: true,
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
    methods: {
        getInitialData() {
            console.log("Get initalData for REFUsol");
            this.dataLoading = true;
            fetch("/api/refusollivedata/status", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.refusolData = data;
                    this.dataLoading = false;
                });
        },
        initSocket() {
            console.log("Starting connection to REFUsol WebSocket Server");

            const { protocol, host } = location;
            const authString = authUrl();
            const webSocketUrl = `${protocol === "https:" ? "wss" : "ws"
                }://${authString}${host}/refusollivedata`;

            this.socket = new WebSocket(webSocketUrl);

            this.socket.onmessage = (event) => {
                console.log(event);
                this.refusolData = JSON.parse(event.data);
                this.dataLoading = false;
                this.heartCheck(); // Reset heartbeat detection
            };

            this.socket.onopen = function (event) {
                console.log(event);
                console.log("Successfully connected to the REFUsol websocket server...");
            };

            // Listen to window events , When the window closes , Take the initiative to disconnect websocket Connect
            window.onbeforeunload = () => {
                this.closeSocket();
            };
        },
        initDataAgeing() {
            this.dataAgeInterval = setInterval(() => {
                if (this.refusolData) {
                    this.refusolData.data_age++;
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
    },
});
</script>
