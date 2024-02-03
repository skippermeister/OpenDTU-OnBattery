<template>
    <BasePage :title="$t('refusoladmin.REFUsolSettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <form @submit="saveREFUsolConfig">
            <CardElement :text="$t('refusoladmin.REFUsolConfiguration')" textVariant="text-bg-primary">
                <InputElement :label="$t('refusoladmin.EnableREFUsol')"
                              v-model="refusolConfigList.enabled"
                              type="checkbox" wide4_1/>

                <div v-show="refusolConfigList.enabled">
                    <InputElement :label="$t('refusoladmin.UpdatesOnly')"
                                  v-model="refusolConfigList.updatesonly"
                                  type="checkbox" wide4_1/>

                    <InputElement :label="$t('refusoladmin.VerboseLogging')"
                                  v-model="refusolConfigList.verbose_logging"
                                  type="checkbox" wide4_1/>
                </div>
            </CardElement>

            <CardElement v-show="refusolConfigList.enabled"
                         :text="$t('refusoladmin.REFUsolParameter')" 
                         textVariant="text-bg-primary"
                         add-space>
                <InputElement :label="$t('refusoladmin.PollInterval')"
                              v-model="refusolConfigList.pollinterval"
                              type="number" min="5" max="60" wide4_2
                              :postfix="$t('refusoladmin.Seconds')"/>
            </CardElement>

            <FormFooter @reload="getREFUsolConfig"/>
        </form>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from "@/components/BootstrapAlert.vue";
import FormFooter from '@/components/FormFooter.vue';
import CardElement from '@/components/CardElement.vue';
import InputElement from '@/components/InputElement.vue';
import type { REFUsolConfig } from "@/types/REFUsolConfig";
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
            refusolConfigList: {} as REFUsolConfig,
            alertMessage: "",
            alertType: "info",
            showAlert: false,
        };
    },
    created() {
        this.getREFUsolConfig();
    },
    methods: {
        getREFUsolConfig() {
            this.dataLoading = true;
            fetch("/api/refusol/config", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.refusolConfigList = data;
                    this.dataLoading = false;
                });
        },
        saveREFUsolConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append("data", JSON.stringify(this.refusolConfigList));

            fetch("/api/refusol/config", {
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