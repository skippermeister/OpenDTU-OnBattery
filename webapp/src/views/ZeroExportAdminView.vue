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
                    <div class="col-sm-8">
                        <table class="table">
                            <thead>
                                <tr>
                                    <th>Auswahl</th>
                                    <th>ID</th>
                                    <th>{{ $t('inverteradmin.Name') }}</th>
                                    <th>{{ $t('inverteradmin.Type') }}</th>
                                    <th>{{ $t('inverteradmin.Serial') }}</th>
                                </tr>
                            </thead>

                            <tbody ref="invList">
                                <tr v-for="(inv, serial) in zeroExportMetaData.inverters" :key="serial" :value="serial">
                                    <td>
                                        <input type="checkbox" id=serial :value="serial" name="serial" v-model="inv.selected" />
                                    </td>
                                    <td>
                                        <label :for="inv.name">{{ inv.pos }}</label>
                                    </td>
                                    <td>{{ inv.name }}</td>
                                    <td>{{ inv.type }}</td>
                                    <td>{{ Number(serial).toString(16) }}</td>
                                </tr>
                            </tbody>
                        </table>
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
import type { ZeroExportConfig, ZeroExportMetaData } from "@/types/ZeroExportConfig";
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
            zeroExportMetaData: {} as ZeroExportMetaData,
            alertMessage: "",
            alertType: "info",
            showAlert: false,
        };
    },
    created() {
        this.getZeroExportConfig();
    },
    methods: {
        getZeroExportConfig() {
            this.dataLoading = true;
            fetch("/api/zeroexport/metadata", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.zeroExportMetaData = data;
                    fetch("/api/zeroexport/config", { headers: authHeader() })
                        .then((response) => handleResponse(response, this.$emitter, this.$router))
                        .then((data) => {
                            this.zeroExportConfigList = data;
                            this.dataLoading = false;
                        });
                });
        },
        saveZeroExportConfig(e: Event) {
            e.preventDefault();

            var cfg = this.zeroExportConfigList;
            var meta = this.zeroExportMetaData;
            var idx=0;

            cfg.serials = [];
            for (const [serial, inverter] of Object.entries(meta.inverters)) {
                if (inverter.selected == true) {
                    cfg.serials[idx++] = serial;
                }
            }

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
