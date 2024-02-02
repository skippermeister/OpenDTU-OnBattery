<template>
    <BasePage :title="$t('acchargeradmin.ChargerSettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <form @submit="saveChargerConfig">
            <CardElement :text="$t('acchargeradmin.ChargerConfiguration')" textVariant="text-bg-primary">
                <InputElement :label="$t('acchargeradmin.EnableMeanWell')"
                              v-model="acChargerConfigList.enabled"
                              type="checkbox" wide4_1/>

                <div v-show="acChargerConfigList.enabled">
                    <InputElement :label="$t('acchargeradmin.UpdatesOnly')"
                              v-model="acChargerConfigList.updatesonly"
                              type="checkbox" wide4_1/>

                    <InputElement :label="$t('acchargeradmin.VerboseLogging')"
                              v-model="acChargerConfigList.verbose_logging"
                              type="checkbox" wide4_1/>
                </div>
            </CardElement>

            <CardElement v-show="acChargerConfigList.enabled"
                         :text="$t('acchargeradmin.ChargerParameter')" 
                         textVariant="text-bg-primary"
                         add-space>
                <InputElement :label="$t('acchargeradmin.PollInterval')"
                              v-model="acChargerConfigList.pollinterval"
                              type="number" min="1" max="300" placeholder="5" wide3_2
                              :postfix="$t('acchargeradmin.Seconds')"/>

                <InputElement :label="$t('acchargeradmin.Min_Voltage')"
                              v-model="acChargerConfigList.min_voltage"
                              type="number" step="0.01" min="42.00" max="54.00" placeholder="47,50" wide3_2
                              :postfix="$t('acchargeradmin.Volts')"/>

                <InputElement :label="$t('acchargeradmin.Max_Voltage')"
                              v-model="acChargerConfigList.max_voltage"
                              type="number" step="0.01" min="42.00" max="54.00" placeholder="53,25" wide3_2
                              :postfix="$t('acchargeradmin.Volts')"/>

                <InputElement :label="$t('acchargeradmin.Min_Current')"
                              v-model="acChargerConfigList.min_current"
                              type="number" step="0.01" min="1.36" max="25.00" placeholder="3,60" wide3_2
                              :postfix="$t('acchargeradmin.Amps')"/>

                <InputElement :label="$t('acchargeradmin.Max_Current')"
                              v-model="acChargerConfigList.max_current"
                              type="number" step="0.01" min="1.36" max="25.00" placeholder="18,00" wide3_2
                              :postfix="$t('acchargeradmin.Amps')"/>

                <InputElement :label="$t('acchargeradmin.Hysteresis')"
                              v-model="acChargerConfigList.hysteresis"
                              type="number" step="1" min="1" max="250" placeholder="1" wide3_2
                              :postfix="$t('acchargeradmin.Watts')"/>
            </CardElement>

            <FormFooter @reload="getChargerConfig"/>
        </form>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from "@/components/BootstrapAlert.vue";
import FormFooter from '@/components/FormFooter.vue';
import CardElement from '@/components/CardElement.vue';
import InputElement from '@/components/InputElement.vue';
import type { AcChargerConfig } from "@/types/AcChargerConfig";
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent } from 'vue';

export default defineComponent({
    components: {
        BasePage,
        BootstrapAlert,
        FormFooter,
        CardElement,
        InputElement,
    },
    data() {
        return {
            dataLoading: true,
            acChargerConfigList: {} as AcChargerConfig,
            alertMessage: "",
            alertType: "info",
            showAlert: false,
        };
    },
    created() {
        this.getChargerConfig();
    },
    methods: {
        getChargerConfig() {
            this.dataLoading = true;
            fetch("/api/meanwell/config", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.acChargerConfigList = data;
                    this.dataLoading = false;
                });
        },
        saveChargerConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append("data", JSON.stringify(this.acChargerConfigList));

            fetch("/api/meanwell/config", {
                method: "POST",
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
