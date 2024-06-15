{
    "extends": [
        "@tsconfig/node18/tsconfig.json",
        "@vue/tsconfig/tsconfig.json"
    ],
    "include": [
        "vite.config.*",
        "vitest.config.*",
        "cypress.config.*"
    ],
    "compilerOptions": {
        "noEmit": false,
        "composite": true,
        "types": [
            "node"
        ]
    }
}
