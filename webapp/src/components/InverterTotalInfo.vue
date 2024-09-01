<template>
    <div class="row row-cols-1 row-cols-md-3 g-3">
        <div class="col" v-if="totalREFUsolData != null && totalREFUsolData.enabled">
            <CardElement centerContent textVariant="text-bg-success"
                :text="$t('invertertotalinfo.REFUsolTotalDeliveredTotal')">
                <h2>
                    {{ $n(totalREFUsolData.total.YieldTotal.v, 'decimal', {
                        minimumFractionDigits: totalREFUsolData.total.YieldTotal.d,
                        maximumFractionDigits: totalREFUsolData.total.YieldTotal.d
                    }) }}
                    <small class="text-muted">{{ totalREFUsolData.total.YieldTotal.u }}</small>
                </h2>
            </CardElement>
        </div>
        <div class="col" v-if="totalREFUsolData != null && totalREFUsolData.enabled">
            <CardElement centerContent textVariant="text-bg-success"
                :text="$t('invertertotalinfo.REFUsolTotalDeliveredDay')">
                <h2>
                    {{ $n(totalREFUsolData.total.YieldDay.v, 'decimal', {
                        minimumFractionDigits: totalREFUsolData.total.YieldDay.d,
                        maximumFractionDigits: totalREFUsolData.total.YieldDay.d
                    }) }}
                    <small class="text-muted">{{ totalREFUsolData.total.YieldDay.u }}</small>
                </h2>
            </CardElement>
        </div>
        <div class="col" v-if="totalREFUsolData != null && totalREFUsolData.enabled">
            <CardElement centerContent textVariant="text-bg-success"
                :text="$t('invertertotalinfo.REFUsolTotalPower')">
                <h2>
                    {{ $n(totalREFUsolData.total.Power.v, 'decimal', {
                        minimumFractionDigits: totalREFUsolData.total.Power.d,
                        maximumFractionDigits: totalREFUsolData.total.Power.d
                    }) }}
                    <small class="text-muted">{{ totalREFUsolData.total.Power.u }}</small>
                </h2>
            </CardElement>
        </div>

        <div class="col" v-if="totalVeData.enabled">
            <CardElement centerContent textVariant="text-bg-success"
                :text="$t('invertertotalinfo.MpptTotalYieldTotal')">
                <h2>
                    {{ $n(totalVeData.total.YieldTotal.v, 'decimal', {
                        minimumFractionDigits: totalVeData.total.YieldTotal.d,
                        maximumFractionDigits: totalVeData.total.YieldTotal.d
                    }) }}
                    <small class="text-muted">{{ totalVeData.total.YieldTotal.u }}</small>
                </h2>
            </CardElement>
        </div>
        <div class="col" v-if="totalVeData.enabled">
            <CardElement centerContent textVariant="text-bg-success"
                :text="$t('invertertotalinfo.MpptTotalYieldDay')">
                <h2>
                    {{ $n(totalVeData.total.YieldDay.v, 'decimal', {
                        minimumFractionDigits: totalVeData.total.YieldDay.d,
                        maximumFractionDigits: totalVeData.total.YieldDay.d
                    }) }}
                    <small class="text-muted">{{ totalVeData.total.YieldDay.u }}</small>
                </h2>
            </CardElement>
        </div>
        <div class="col" v-if="totalVeData.enabled">
            <CardElement centerContent textVariant="text-bg-success"
                :text="$t('invertertotalinfo.MpptTotalPower')">
                <h2>
                    {{ $n(totalVeData.total.Power.v, 'decimal', {
                        minimumFractionDigits: totalVeData.total.Power.d,
                        maximumFractionDigits: totalVeData.total.Power.d
                    }) }}
                    <small class="text-muted">{{ totalVeData.total.Power.u }}</small>
                </h2>
            </CardElement>
        </div>

        <div class="col">
            <CardElement centerContent textVariant="text-bg-success"
                :text="$t('invertertotalinfo.InverterTotalYieldTotal')">
                <h2>
                    {{ $n(totalData.YieldTotal.v, 'decimal', {
                        minimumFractionDigits: totalData.YieldTotal.d,
                        maximumFractionDigits: totalData.YieldTotal.d
                    }) }}
                    <small class="text-muted">{{ totalData.YieldTotal.u }}</small>
                </h2>
            </CardElement>
        </div>
        <div class="col">
            <CardElement centerContent textVariant="text-bg-success"
                :text="$t('invertertotalinfo.InverterTotalYieldDay')">
                <h2>
                    {{ $n(totalData.YieldDay.v, 'decimal', {
                        minimumFractionDigits: totalData.YieldDay.d,
                        maximumFractionDigits: totalData.YieldDay.d
                    }) }}
                    <small class="text-muted">{{ totalData.YieldDay.u }}</small>
                </h2>
            </CardElement>
        </div>
        <div class="col">
            <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.InverterTotalPower')">
                <h2>
                    {{ $n(totalData.Power.v, 'decimal', {
                        minimumFractionDigits: totalData.Power.d,
                        maximumFractionDigits: totalData.Power.d
                    }) }}
                    <small class="text-muted">{{ totalData.Power.u }}</small>
                </h2>
            </CardElement>
        </div>

        <div class="col" v-if="powerMeterData.enabled">
            <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.GridPower')">
                <h2>
                    {{ $n(powerMeterData.GridPower.v, 'decimal', {
                        minimumFractionDigits: powerMeterData.GridPower.d,
                        maximumFractionDigits: powerMeterData.GridPower.d
                    }) }}
                    <small class="text-muted">{{ powerMeterData.GridPower.u }}</small>
                </h2>
            </CardElement>
        </div>
        <div class="col" v-if="powerMeterData.enabled">
            <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.HousePower')">
                <h2>
                    {{ $n(powerMeterData.HousePower.v, 'decimal', {
                        minimumFractionDigits: powerMeterData.HousePower.d,
                        maximumFractionDigits: powerMeterData.HousePower.d
                    }) }}
                    <small class="text-muted">{{ powerMeterData.HousePower.u }}</small>
                </h2>
            </CardElement>
        </div>
        <div class="col" v-if="chargerData.enabled">
            <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.ChargerPower')">
                <h2>
                    {{ $n(chargerData.Power.v, 'decimal', {
                        minimumFractionDigits: chargerData.Power.d,
                        maximumFractionDigits: chargerData.Power.d
                    }) }}
                    <small class="text-muted">{{ chargerData.Power.u }}</small>
                </h2>
            </CardElement>
        </div>

        <template v-if="totalBattData.enabled">
            <div class="col">
                <CardElement centerContent flexChildren textVariant="text-bg-success"
                    :text="$t('invertertotalinfo.BatteryCharge')">
                    <div class="flex-fill" v-if="totalBattData.soc">
                        <h2>
                            {{ $n(totalBattData.soc.v, 'decimal', {
                                minimumFractionDigits: totalBattData.soc.d,
                                maximumFractionDigits: totalBattData.soc.d
                            }) }}
                            <small class="text-muted">{{ totalBattData.soc.u }}</small>
                        </h2>
                    </div>

                    <div class="flex-fill" v-if="totalBattData.voltage">
                        <h2>
                            {{ $n(totalBattData.voltage.v, 'decimal', {
                                minimumFractionDigits: totalBattData.voltage.d,
                                maximumFractionDigits: totalBattData.voltage.d
                            }) }}
                            <small class="text-muted">{{ totalBattData.voltage.u }}</small>
                        </h2>
                    </div>
                </CardElement>
            </div>
            <div class="col" v-if="totalBattData.power || totalBattData.current">
                <CardElement centerContent flexChildren textVariant="text-bg-success"
                    :text="$t('invertertotalinfo.BatteryPower')">
                    <div class="flex-fill" v-if="totalBattData.power">
                        <h2>
                            {{ $n(totalBattData.power.v, 'decimal', {
                                minimumFractionDigits: totalBattData.power.d,
                                maximumFractionDigits: totalBattData.power.d
                            }) }}
                            <small class="text-muted">{{ totalBattData.power.u }}</small>
                        </h2>
                    </div>

                    <div class="flex-fill" v-if="totalBattData.current">
                        <h2>
                            {{ $n(totalBattData.current.v, 'decimal', {
                                minimumFractionDigits: totalBattData.current.d,
                                maximumFractionDigits: totalBattData.current.d
                            }) }}
                            <small class="text-muted">{{ totalBattData.current.u }}</small>
                        </h2>
                    </div>
                </CardElement>
            </div>
        </template>
    </div>
</template>

<script lang="ts">
import type { Battery, Total, Vedirect, REFUsol, Charger, PowerMeter } from '@/types/LiveDataStatus';
import CardElement from './CardElement.vue';
import { defineComponent, type PropType } from 'vue';

export default defineComponent({
    components: {
        CardElement,
    },
    props: {
        totalData: { type: Object as PropType<Total>, required: true },
        totalVeData: { type: Object as PropType<Vedirect>, required: true },
        totalREFUsolData: { type: Object as PropType<REFUsol>, required: true },
        totalBattData: { type: Object as PropType<Battery>, required: true },
        powerMeterData: { type: Object as PropType<PowerMeter>, required: true },
        chargerData: { type: Object as PropType<Charger>, required: true },
    },
});
</script>
