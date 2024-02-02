<template>
    <div class="row">
        <div class="col">
            <div class="card">
                <div class="card-header text-bg-success">{{ $t('ntpadmin.CurrentLocalTime') }} {{
                    currentDateTime()
                    }}
                </div>
                <div class="card-body">
                    <GChart
                            :type="chartType"
                            :data="chartData"
                            :options="chartOptions"
                    ></GChart>
                </div>
            </div>
        </div>
    </div>
</template>

<script setup lang="ts">
import {GChart} from 'vue-google-charts';
import {type PropType, computed} from 'vue';
import type {ArrayValueObject} from "@/types/LiveDataStatus";
import type {GoogleChartWrapperChartType} from "vue-google-charts/dist/types";
import {dateTimeFormats, Locales} from "@/locales";

import {useI18n} from "vue-i18n";
const {t} = useI18n();

const translation = computed(() => {
    return {
        YieldUnit: t("hourschart.YieldUnit"),
    }
})

const theme = localStorage.getItem('theme');

const props = defineProps({
    data: {type: Object as PropType<ArrayValueObject>, required: true},
});

const colorMappings: { [key: string]: { [key: string]: string } } = {
    light: {
        background: '#FFFFFF',
        font: '#21252A',
    },
    dark: {
        background: '#2B3035',
        font: '#DEE2E6',
    },
};

const chartType: GoogleChartWrapperChartType = 'ColumnChart';

const chartData = computed(() => {
    const data = [["Hour", "Power", {role: 'style'}]];
    props.data?.values.forEach((value, index) => {
        // eslint-disable-next-line @typescript-eslint/ban-ts-comment
        // @ts-ignore
        data.push([index.toString(), value, '#75b798; opacity: 1']);
    });

    return data
})

const chartOptions = computed(() => {
    return {
        backgroundColor: backgroundColor(),
        fontColor: fontColor(),
        legend: {position: 'none'},
        height: 300,
        titleTextStyle: {
            color: backgroundColor()
        },
        chartArea: {
            backgroundColor: backgroundColor()
        },
        hAxis: {
            textStyle: {
                color: fontColor()
            },
            titleTextStyle: {
                color: fontColor()
            },
            viewWindow: {
                min: 0
            }
        },
        vAxis: {
            title: translation.value.YieldUnit,
            textStyle: {
                color: fontColor()
            },
            titleTextStyle: {
                color: fontColor()
            },
            viewWindow: {
                min: 0
            }
        },
        showScale: false
    };
});


function fontColor() {
    return getColor('font');
}

function backgroundColor() {
    return getColor('background');
}

function getColor(type: string) {
    if (theme && colorMappings[theme]) {
        return colorMappings[theme][type] || colorMappings.light[type];
    } else {
        return colorMappings.light[type];
    }
}

const dateTimeFormat = new Intl.DateTimeFormat(Locales.DE, dateTimeFormats![Locales.DE]['datetime']);

function currentDateTime() {
    return dateTimeFormat.format(new Date());
}

</script>

<style>

</style>
