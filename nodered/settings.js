module.exports = {
    flowFile: "flows.json",
    flowFilePretty: true,

    uiPort: process.env.PORT || 1880,

    // Editor de flows acessível apenas em /admin-red (não linkado publicamente
    // pelo Nginx — ver docs/deploy.md para acesso via túnel SSH).
    httpAdminRoot: "/admin-red",
    httpNodeRoot: "/",

    // Serve o portal web estático (web/) diretamente pelo Node-RED
    httpStatic: "/data/web",

    credentialSecret: process.env.NODE_RED_CREDENTIAL_SECRET || "smartpark-red-default-secret",

    logging: {
        console: {
            level: "info",
            metrics: false,
            audit: false
        }
    },

    editorTheme: {
        projects: { enabled: false }
    },

    functionGlobalContext: {},

    exportGlobalContextKeys: false,

    contextStorage: {
        default: { module: "localfilesystem" }
    }
};
