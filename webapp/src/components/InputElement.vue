<template>
    <div class="row mb-3">
        <label
            :for="inputId"
            :class="[ wide5_1 || wide5_2 || wide5_3 || wide5_4 || wide5_7 ? 'col-sm-5'
                    : wide4_1 || wide4_2 || wide4_3 || wide4_4 || wide4_8 ? 'col-sm-4'
                    : wide3_1 || wide3_2 || wide3_3 || wide3_4 || wide3_5 || wide3_6 || wide3_9 ? 'col-sm-3'
                    : 'col-sm-2', isCheckbox ? 'form-check-label' : 'col-form-label' ]"
        >
            {{ label }}
            <BIconInfoCircle v-if="tooltip !== undefined" v-tooltip :title="tooltip" />
        </label>
        <div :class="[ wide2_1 || wide3_1 || wide4_1 || wide5_1 ? 'col-sm-1'
                    : wide2_2 || wide3_2 || wide4_2 || wide5_2 ? 'col-sm-2'
                    : wide2_3 || wide3_3 || wide4_3 || wide5_3  ? 'col-sm-3'
                    : wide2_4 || wide3_4 || wide4_4 || wide5_4? 'col-sm-4'
                    : wide2_10 || wide ? 'col-sm-10'
                    : wide3_5 ? 'col-sm-5'
                    : wide3_6 ? 'col-sm-6'
                    : wide3_9 ? 'col-sm-9'
                    : wide4_8 ? 'col-sm-8'
                    : 'col-sm-7']">
            <div v-if="!isTextarea"
                 :class="{'form-check form-switch': isCheckbox,
                          'input-group': postfix || prefix }"
            >
                 <span v-if="prefix"
                       class="input-group-text"
                       :id="descriptionId"
                 >
                    {{ prefix }}
                </span>
                <input
                    v-model="model"
                    :class="[ isCheckbox ? 'form-check-input' : 'form-control' ]"
                    :id="inputId"
                    :placeholder="placeholder"
                    :type="type"
                    :maxlength="maxlength"
                    :min="min"
                    :max="max"
                    :step="step"
                    :disabled="disabled"
                    :aria-describedby="descriptionId"
                />
                <span v-if="postfix"
                      class="input-group-text"
                      :id="descriptionId"
                >
                    {{ postfix }}
                </span>
                <slot/>
            </div>
            <div v-else>
                <textarea
                    v-model="model"
                    class="form-control"
                    :id="inputId"
                    :maxlength="maxlength"
                    :rows="rows"
                    :disabled="disabled"
                    :placeholder="placeholder"
                />
            </div>
        </div>
    </div>
</template>

<script lang="ts">
import { BIconInfoCircle } from 'bootstrap-icons-vue';
import { defineComponent } from 'vue';

export default defineComponent({
    components: {
        BIconInfoCircle,
    },
    props: {
        'modelValue': [String, Number, Boolean, Date],
        'modelNumberValue': Number,
        'label': String,
        'placeholder': String,
        'type': String,
        'maxlength': String,
        'min': String,
        'max': String,
        'step': String,
        'rows': String,
        'disabled': Boolean,
        'postfix': String,
        'prefix': String,
        'wide': Boolean,
        'wide2_1': Boolean,
        'wide2_2': Boolean,
        'wide2_3': Boolean,
        'wide2_4': Boolean,
        'wide2_10': Boolean,
        'wide3_1': Boolean,
        'wide3_2': Boolean,
        'wide3_3': Boolean,
        'wide3_4': Boolean,
        'wide3_5': Boolean,
        'wide3_6': Boolean,
        'wide3_9': Boolean,
        'wide4_1': Boolean,
        'wide4_2': Boolean,
        'wide4_3': Boolean,
        'wide4_4': Boolean,
        'wide4_8': Boolean,
        'wide5_1': Boolean,
        'wide5_2': Boolean,
        'wide5_3': Boolean,
        'wide5_4': Boolean,
        'wide5_7': Boolean,
        'tooltip': String,
    },
    data() {
        return {};
    },
    computed: {
        model: {
            // eslint-disable-next-line @typescript-eslint/no-explicit-any
            get(): any {
                if (this.type === 'checkbox') return !!this.modelValue;
                else if(this.type === 'number') {
                    let p = this.modelValue?.toString();
                    if (p == undefined) p = "0.0";
                    if (this.step == '1') return parseFloat(p).toFixed(0);
                    if (this.step == '0.1') return parseFloat(p).toFixed(1);
                    else if (this.step == '0.01') return parseFloat(p).toFixed(2);
                    else if (this.step == '0.001') return parseFloat(p).toFixed(3);
                    return this.modelValue;
                }
                return this.modelValue;
            },
            // eslint-disable-next-line @typescript-eslint/no-explicit-any
            set(value: any) {
                this.$emit('update:modelValue', value);
            },
        },
        uniqueLabel() {
            // normally, the label is sufficient to build a unique id
            // if two inputs with the same label text on one page is required,
            // use a unique placeholder even if it is a checkbox
            return this.label?.replace(/[^A-Za-z0-9]/g, '') +
                (this.placeholder ? this.placeholder.replace(/[^A-Za-z0-9]/g, '') : '');
        },
        inputId() {
            return 'input' + this.uniqueLabel
        },
        descriptionId() {
            return 'desc' + this.uniqueLabel;
        },
        isTextarea() {
            return this.type === 'textarea';
        },
        isCheckbox() {
            return this.type === 'checkbox';
        }
    },
});
</script>
