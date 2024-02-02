<template>
    <BasePage :title="$t('mqttinfo.MqttInformation')" :isLoading="dataLoading" :show-reload="true" @reload="getMqttInfo">
        <CardElement :text="$t('mqttinfo.ConfigurationSummary')" textVariant="text-bg-primary">
            <div class="table-responsive">
                <table class="table table-hover table-condensed">
                    <tbody>
                        <tr>
                            <th>{{ $t('mqttinfo.Status') }}</th>
                            <td>
                                <StatusBadge :status="mqttDataList.enabled" true_text="mqttinfo.Enabled" false_text="mqttinfo.Disabled" />
                            </td>
                        </tr>
                        <tr>
                            <th>{{ $t('mqttinfo.Server') }}</th>
                            <td>{{ mqttDataList.hostname }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('mqttinfo.Port') }}</th>
                            <td>{{ mqttDataList.port }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('mqttinfo.Username') }}</th>
                            <td>{{ mqttDataList.username }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('mqttinfo.BaseTopic') }}</th>
                            <td>{{ mqttDataList.topic }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('mqttinfo.PublishInterval') }}</th>
                            <td>{{ $t('mqttinfo.Seconds', { sec: mqttDataList.publish_interval }) }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('mqttinfo.CleanSession') }}</th>
                            <td>
                                <StatusBadge :status="mqttDataList.clean_session" true_text="mqttinfo.Enabled" false_text="mqttinfo.Disabled" />
                            </td>
                        </tr>
                        <tr>
                            <th>{{ $t('mqttinfo.Retain') }}</th>
                            <td>
                                <StatusBadge :status="mqttDataList.retain" true_text="mqttinfo.Enabled" false_text="mqttinfo.Disabled" />
                            </td>
                        </tr>
                        <tr>
                            <th>{{ $t('mqttinfo.Tls') }}</th>
                            <td>
                                <StatusBadge :status="mqttDataList.tls" true_text="mqttinfo.Enabled" false_text="mqttinfo.Disabled" />
                            </td>
                        </tr>
                        <tr v-show="mqttDataList.tls">
                            <th>{{ $t('mqttinfo.RootCertifcateInfo') }}</th>
                            <td>{{ mqttDataList.root_ca_cert_info }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('mqttinfo.TlsCertLogin') }}</th>
                            <td>
                                <StatusBadge :status="mqttDataList.tls_cert_login" true_text="mqttinfo.Enabled" false_text="mqttinfo.Disabled" />
                            </td>
                        </tr>
                        <tr v-show="mqttDataList.tls_cert_login">
                            <th>{{ $t('mqttinfo.ClientCertifcateInfo') }}</th>
                            <td>{{ mqttDataList.client_cert_info }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('mqttinfo.VerboseLogging') }}</th>
                            <td>
                                <StatusBadge :status="mqttDataList.verbose_logging" true_text="mqttinfo.Enabled" false_text="mqttinfo.Disabled" />
                            </td>
                        </tr>
                    </tbody>
                </table>
            </div>
        </CardElement>

        <CardElement :text="$t('mqttinfo.HassSummary')" textVariant="text-bg-primary" add-space>
            <div class="table-responsive">
                <table class="table table-hover table-condensed">
                    <tbody>
                        <tr>
                            <th>{{ $t('mqttinfo.Status') }}</th>
                            <td>
                                <StatusBadge :status="mqttDataList.hass_enabled" true_text="mqttinfo.Enabled" false_text="mqttinfo.Disabled" />
                            </td>
                        </tr>
                        <tr>
                            <th>{{ $t('mqttinfo.BaseTopic') }}</th>
                            <td>{{ mqttDataList.hass_topic }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('mqttinfo.Retain') }}</th>
                            <td>
                                <StatusBadge :status="mqttDataList.hass_retain" true_text="mqttinfo.Enabled" false_text="mqttinfo.Disabled" />
                            </td>
                        </tr>
                        <tr>
                            <th>{{ $t('mqttinfo.Expire') }}</th>
                            <td>
                                <StatusBadge :status="mqttDataList.hass_expire" true_text="mqttinfo.Enabled" false_text="mqttinfo.Disabled" />
                            </td>
                        </tr>
                        <tr>
                            <th>{{ $t('mqttinfo.IndividualPanels') }}</th>
                            <td>
                                <StatusBadge :status="mqttDataList.hass_individualpanels" true_text="mqttinfo.Enabled" false_text="mqttinfo.Disabled" />
                            </td>
                        </tr>
                    </tbody>
                </table>
            </div>
        </CardElement>

        <CardElement :text="$t('mqttinfo.RuntimeSummary')" textVariant="text-bg-primary" add-space>
            <div class="table-responsive">
                <table class="table table-hover table-condensed">
                    <tbody>
                        <tr>
                            <th>{{ $t('mqttinfo.ConnectionStatus') }}</th>
                            <td>
                                <StatusBadge :status="mqttDataList.connected" true_text="mqttinfo.Connected" false_text="mqttinfo.Disconnected" />
                            </td>
                        </tr>
                    </tbody>
                </table>
            </div>
        </CardElement>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import CardElement from '@/components/CardElement.vue';
import StatusBadge from '@/components/StatusBadge.vue';
import type { MqttStatus } from '@/types/MqttStatus';
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent } from 'vue';

export default defineComponent({
    components: {
        BasePage,
        CardElement,
        StatusBadge
    },
    data() {
        return {
            dataLoading: true,
            mqttDataList: {} as MqttStatus,
        };
    },
    created() {
        this.getMqttInfo();
    },
    methods: {
        getMqttInfo() {
            this.dataLoading = true;
            fetch("/api/mqtt/status", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.mqttDataList = data;
                    this.dataLoading = false;
                });
        },
    },
});
</script>
