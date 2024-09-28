<template>
    <BasePage :title="$t('refusolinfo.REFUsolInformation')" :isLoading="dataLoading">
        <CardElement :text="$t('refusolinfo.ConfigurationSummary')" textVariant="text-bg-primary">
            <div class="table-responsive">
                <table class="table table-hover table-condensed">
                    <tbody>
                        <tr>
                            <th>{{ $t('refusolinfo.Status') }}</th>
                            <td>
                                <StatusBadge
                                    :status="refusolDataList.enabled"
                                    true_text="refusolinfo.Enabled"
                                    false_text="refusolinfo.Disabled"
                                />
                            </td>
                        </tr>
                        <tr>
                            <th>{{ $t('refusolinfo.UpdatesOnly') }}</th>
                            <td>
                                <StatusBadge
                                    :status="refusolDataList.updatesonly"
                                    true_text="refusolinfo.Enabled"
                                    false_text="refusolinfo.Disabled"
                                />
                            </td>
                        </tr>
                        <tr>
                            <th>{{ $t('refusolinfo.USSaddress') }}</th>
                            <td>{{ refusolDataList.uss_address }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('refusolinfo.Baudrate') }}</th>
                            <td>{{ refusolDataList.baudrate }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('refusolinfo.Parity') }}</th>
                            <td>{{ getParity(refusolDataList.parity) }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('refusolinfo.VerboseLogging') }}</th>
                            <td>
                                <StatusBadge
                                    :status="refusolDataList.verbose_logging"
                                    true_text="refusolinfo.Enabled"
                                    false_text="refusolinfo.Disabled"
                                />
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
import type { REFUsolStatus } from '@/types/REFUsolStatus';
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent } from 'vue';

export default defineComponent({
    components: {
        BasePage,
        CardElement,
        StatusBadge,
    },
    data() {
        return {
            dataLoading: true,
            refusolDataList: {} as REFUsolStatus,
        };
    },
    created() {
        this.getREFUsolInfo();
    },
    methods: {
        getREFUsolInfo() {
            this.dataLoading = true;
            fetch('/api/refusol/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.refusolDataList = data;
                    this.dataLoading = false;
                });
        },
        getParity(value: number) {
            if (value == 0) return 'None';
            else if (value == 1) return 'Even';
            else if (value == 2) return 'Odd';
            else return 'undefined';
        },
    },
});
</script>
