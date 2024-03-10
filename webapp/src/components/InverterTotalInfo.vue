<template>
    <div v-show="totalREFUsolData.enabled">
        <div v-if="totalREFUsolData.total !== undefined" class="row row-cols-1 row-cols-md-3 g-3">
            <div class="col">
                <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.REFUsolTotalDeliveredTotal')">
                    <h2>
                        {{ $n(totalREFUsolData.total.YieldTotal.v, 'decimal', {
                            minimumFractionDigits: totalREFUsolData.total.YieldTotal.d,
                            maximumFractionDigits: totalREFUsolData.total.YieldTotal.d
                        }) }}
                        <small class="text-muted">{{ totalREFUsolData.total.YieldTotal.u }}</small>
                    </h2>
                </CardElement>
            </div>
            <div class="col">
                <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.REFUsolTotalDeliveredDay')">
                    <h2>
                        {{ $n(totalREFUsolData.total.YieldDay.v, 'decimal', {
                            minimumFractionDigits: totalREFUsolData.total.YieldDay.d,
                            maximumFractionDigits: totalREFUsolData.total.YieldDay.d
                        }) }}
                      <small class="text-muted">{{ totalREFUsolData.total.YieldDay.u }}</small>
                    </h2>
                </CardElement>
            </div>
            <div class="col">
                <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.REFUsolTotalPower')">
                    <h2>
                        {{ $n(totalREFUsolData.total.Power.v, 'decimal', {
                            minimumFractionDigits: totalREFUsolData.total.Power.d,
                            maximumFractionDigits: totalREFUsolData.total.Power.d
                        }) }}
                        <small class="text-muted">{{ totalREFUsolData.total.Power.u }}</small>
                    </h2>
                </CardElement>
            </div>
        </div>
    </div>
    <div v-show="totalVeData.enabled">
        <div class="row row-cols-1 row-cols-md-3 g-3">
            <div class="col">
                <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.MpptTotalYieldTotal')">
                    <h2>
                        {{ $n(totalVeData.total.YieldTotal.v, 'decimal', {
                            minimumFractionDigits: totalVeData.total.YieldTotal.d,
                            maximumFractionDigits: totalVeData.total.YieldTotal.d
                        }) }}
                        <small class="text-muted">{{ totalVeData.total.YieldTotal.u }}</small>
                    </h2>
                </CardElement>
            </div>
            <div class="col">
                <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.MpptTotalYieldDay')">
                    <h2>
                        {{ $n(totalVeData.total.YieldDay.v, 'decimal', {
                            minimumFractionDigits: totalVeData.total.YieldDay.d,
                            maximumFractionDigits: totalVeData.total.YieldDay.d
                        }) }}
                        <small class="text-muted">{{ totalVeData.total.YieldDay.u }}</small>
                    </h2>
                </CardElement>
            </div>
            <div class="col">
                <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.MpptTotalPower')">
                    <h2>
                        {{ $n(totalVeData.total.Power.v, 'decimal', {
                            minimumFractionDigits: totalVeData.total.Power.d,
                            maximumFractionDigits: totalVeData.total.Power.d
                        }) }}
                        <small class="text-muted">{{ totalVeData.total.Power.u }}</small>
                    </h2>
                </CardElement>
            </div>
        </div>
    </div>

    <div class="row row-cols-1 row-cols-md-3 g-3">
        <div class="col">
            <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.InverterTotalYieldTotal')">
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
            <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.InverterTotalYieldDay')">
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
    </div>

    <div v-show="powerMeterData.enabled || meanwellData.enabled">
        <div class="row row-cols-1 row-cols-md-3 g-3">
            <div v-show="powerMeterData.enabled" class="col">
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
            <div v-show="powerMeterData.enabled" class="col">
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
            <div v-show="meanwellData.enabled" class="col">
                <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.MeanWellPower')">
                    <h2>
                        {{ $n(meanwellData.Power.v, 'decimal', {
                            minimumFractionDigits: meanwellData.Power.d,
                            maximumFractionDigits: meanwellData.Power.d
                        }) }}
                        <small class="text-muted">{{ meanwellData.Power.u }}</small>
                    </h2>
                </CardElement>
            </div>
        </div>
    </div>

    <div v-show="totalBattData.enabled">
        <div class="row row-cols-1 row-cols-md-3 g-3">
            <div class="col">
                <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.BatterySoc')">
                    <h2>
                        {{ $n(totalBattData.soc.v, 'decimal', {
                            minimumFractionDigits: totalBattData.soc.d,
                            maximumFractionDigits: totalBattData.soc.d
                        }) }}
                        <small class="text-muted">{{ totalBattData.soc.u }}</small>
                    </h2>
                </CardElement>
            </div>
        </div>
    </div>
</template>

<script lang="ts">
import type { Battery, Total, Vedirect, REFUsol, MeanWell, PowerMeter } from '@/types/LiveDataStatus';
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
        meanwellData: { type: Object as PropType<MeanWell>, required: true },
    },
});
</script>
