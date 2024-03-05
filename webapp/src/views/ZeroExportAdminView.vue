<template>
    <BasePage :title="$t('zeroexportadmin.ZeroExportSettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <form @submit="saveZeroExportConfig">
            <CardElement :text="$t('zeroexportadmin.ZeroExportConfiguration')" textVariant="text-bg-primary">
                <InputElement :label="$t('zeroexportadmin.EnableZeroExport')"
                              v-model="zeroExportConfigList.enabled"
                              type="checkbox" wide4_1/>

                <div v-show="zeroExportConfigList.enabled">
                    <InputElement :label="$t('zeroexportadmin.UpdatesOnly')"
                                  v-model="zeroExportConfigList.updatesonly"
                                  type="checkbox" wide4_1/>

                    <InputElement :label="$t('zeroexportadmin.VerboseLogging')"
                                  v-model="zeroExportConfigList.verbose_logging"
                                  type="checkbox" wide4_1/>
                </div>
            </CardElement>

            <CardElement :text="$t('zeroexportadmin.ZeroExportParameter')" textVariant="text-bg-primary" add-space
                         v-show="zeroExportConfigList.enabled">

                <div class="row mb-3">
                    <label for="zeroexportInverterlist" class="col-sm-4 col-form-label">
                        {{ $t('zeroexportadmin.InverterId') }}:
                        <BIconInfoCircle v-tooltip :title="$t('zeroexportadmin.InverterIdHint')" />
                    </label>
                    <div class="col-sm-2">
                        <select class="form-select" v-model="zeroExportConfigList.InverterId">
                            <option v-for="inverter in inverterList" :key="inverter.key" :value="inverter.key">
                                {{ inverter.value }}
                            </option>
                        </select>
                    </div>
                </div>

                <InputElement :label="$t('zeroexportadmin.MaxGrid')"
                              v-model="zeroExportConfigList.MaxGrid"
                              type="number" min="0" max="25000" wide4_2
                              :postfix="$t('zeroexportadmin.Watts')"/>

                <InputElement :label="$t('zeroexportadmin.PowerHysteresis')"
                              v-model="zeroExportConfigList.PowerHysteresis"
                              type="number" min="1" max="50" wide4_2
                              :postfix="$t('zeroexportadmin.Percent')"/>

                <InputElement :label="$t('zeroexportadmin.MinimumLimit')"
                              v-model="zeroExportConfigList.MinimumLimit"
                              type="number" min="2" max="50" wide4_2
                              :postfix="$t('zeroexportadmin.Percent')"/>

                <InputElement :label="$t('zeroexportadmin.Tn')"
                              v-model="zeroExportConfigList.Tn"
                              type="number" min="1" max="300" wide4_2
                              :postfix="$t('zeroexportadmin.Seconds')"/>

            </CardElement>

            <FormFooter @reload="getZeroExportConfig"/>
        </form>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from "@/components/BootstrapAlert.vue";
import FormFooter from '@/components/FormFooter.vue';
import CardElement from '@/components/CardElement.vue';
import InputElement from '@/components/InputElement.vue';
import type { ZeroExportConfig } from "@/types/ZeroExportConfig";
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
            zeroExportConfigList: {} as ZeroExportConfig,
            alertMessage: "",
            alertType: "info",
            showAlert: false,
            inverterList: [
                { key: 1, value: "ID 00" },
                { key: 2, value: "ID 01" },
                { key: 4, value: "ID 02" },
                { key: 1+2, value: "ID 00 + ID 01" },
                { key: 1+4, value: "ID 00 + ID 02" },
                { key: 2+4, value: "ID 01 + ID 02" },
//              { key: 1+2+4+0, value: "ID 00 + ID 01 + ID 02" },
//              { key: 1+2+0+8, value: "ID 00 + ID 01 + ID 03" },
//              { key: 1+0+4+8, value: "ID 00 + ID 02 + ID 03" },
//              { key: 0+2+4+8, value: "ID 01 + ID 02 + ID 03" },
//              { key: 0+2+0+8, value: "ID 01 + ID 03" },
//              { key: 0+0+4+8, value: "ID 02 + ID 03" },
//              { key: 0+2+4+0+16, value: "ID 01 + ID 02 + ID 04" },
//              { key: 8, value: "ID 03" },
//              { key: 16, value: "ID 04" },
//              { key: 32, value: "ID 05" },
//              { key: 64, value: "ID 06" },
//              { key: 128, value: "ID 07" },
//              { key: 256, value: "ID 08" },
//              { key: 512, value: "ID 09" },
            ],
        };
    },
    created() {
        this.getZeroExportConfig();
    },
    methods: {
        getZeroExportConfig() {
            this.dataLoading = true;
            fetch("/api/zeroexport/config", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.zeroExportConfigList = data;
                    this.dataLoading = false;
                });
        },
        saveZeroExportConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append("data", JSON.stringify(this.zeroExportConfigList));

            fetch("/api/zeroexport/config", {
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
