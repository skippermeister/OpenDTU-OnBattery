<template>
    <BasePage :title="$t('refusoladmin.REFUsolSettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <form @submit="saveREFUsolConfig">
            <CardElement :text="$t('refusoladmin.REFUsolConfiguration')" textVariant="text-bg-primary">
                <InputElement
                    :label="$t('refusoladmin.EnableREFUsol')"
                    v-model="refusolConfigList.enabled"
                    type="checkbox"
                    wide4_1
                />

                <div v-show="refusolConfigList.enabled">
                    <InputElement
                        :label="$t('refusoladmin.UpdatesOnly')"
                        v-model="refusolConfigList.updatesonly"
                        type="checkbox"
                        wide4_1
                    />

                    <InputElement
                        :label="$t('refusoladmin.VerboseLogging')"
                        v-model="refusolConfigList.verbose_logging"
                        type="checkbox"
                        wide4_1
                    />
                </div>
            </CardElement>

            <CardElement
                v-show="refusolConfigList.enabled"
                :text="$t('refusoladmin.REFUsolParameter')"
                textVariant="text-bg-primary"
                add-space
            >
                <InputElement
                    :label="$t('refusoladmin.PollInterval')"
                    v-model="refusolConfigList.pollinterval"
                    type="number"
                    min="1"
                    max="60"
                    wide3_2
                    :postfix="$t('refusoladmin.Seconds')"
                />

                <InputElement
                    :label="$t('refusoladmin.USSaddress')"
                    v-model="refusolConfigList.uss_address"
                    type="number"
                    min="1"
                    max="31"
                    wide3_1
                />

                <div class="row mb-3">
                    <label for="baudrate" class="col-sm-3 col-form-label">
                        {{ $t('refusoladmin.Baudrate') }}
                    </label>
                    <div class="col-sm-2">
                        <select id="baudrate" class="form-select" v-model="refusolConfigList.baudrate">
                            <option v-for="u in baudrateList" :key="u.key" :value="u.key">
                                {{ u.value }}
                            </option>
                        </select>
                    </div>
                </div>

                <div class="row mb-3">
                    <label for="parity" class="col-sm-3 col-form-label">
                        {{ $t('refusoladmin.Parity') }}
                    </label>
                    <div class="col-sm-2">
                        <select id="parity" class="form-select" v-model="refusolConfigList.parity">
                            <option v-for="u in parityList" :key="u.key" :value="u.key">
                                {{ u.value }}
                            </option>
                        </select>
                    </div>
                </div>
            </CardElement>

            <FormFooter @reload="getREFUsolConfig" />
        </form>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import FormFooter from '@/components/FormFooter.vue';
import CardElement from '@/components/CardElement.vue';
import InputElement from '@/components/InputElement.vue';
import type { REFUsolConfig } from '@/types/REFUsolConfig';
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
            alertMessage: '',
            alertType: 'info',
            showAlert: false,
            baudrateList: [
                { key: 19200, value: '19200' },
                { key: 38400, value: '38400' },
                { key: 57600, value: '57600' },
                { key: 115200, value: '115200' },
            ],
            parityList: [
                { key: 0, value: 'None' },
                { key: 1, value: 'Even' },
                { key: 2, value: 'Odd' },
            ],
        };
    },
    created() {
        this.getREFUsolConfig();
    },
    methods: {
        getREFUsolConfig() {
            this.dataLoading = true;
            fetch('/api/refusol/config', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.refusolConfigList = data;
                    this.dataLoading = false;
                });
        },
        saveREFUsolConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append('data', JSON.stringify(this.refusolConfigList));

            fetch('/api/refusol/config', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    this.alertMessage = this.$t('apiresponse.' + response.code, response.param);
                    this.alertType = response.type;
                    this.showAlert = true;
                });
        },
    },
});
</script>
