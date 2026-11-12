// The schema view has already removed validation-only constraints. The full
// Draft 7 schema remains the sole runtime validation contract.
const fs = require("node:fs");
const { quicktype, InputData, JSONSchemaInput, FetchingJSONSchemaStore, Ref } = require("quicktype-core");

(async () => {
    const { schema, models } = JSON.parse(fs.readFileSync(0, "utf8"));
    const output = {};
    for (const lang of ["python", "cpp"]) {
        const input = new JSONSchemaInput(new FetchingJSONSchemaStore());
        const address = "asset-protocol.json";
        // Share one schema graph, with each public model as its own top level.
        await input.addSource({ name: models[0], schema: JSON.stringify(schema),
            uris: [`${address}#/definitions/${models[0]}`] });
        for (const name of models) input.addTopLevel(name, Ref.parse(`${address}#/definitions/${name}`));
        const inputData = new InputData();
        inputData.addInput(input);
        const rendererOptions = lang === "python"
            ? { "just-types": true, "python-version": "3.10" }
            : { namespace: "asset_protocol", "code-format": "with-struct", "include-location": "global-include", boost: false };
        const result = await quicktype({ lang, inputData, combineClasses: true, rendererOptions });
        output[lang] = result.lines.join("\n") + "\n";
    }
    process.stdout.write(JSON.stringify(output));
})().catch(error => { console.error(error); process.exitCode = 1; });
