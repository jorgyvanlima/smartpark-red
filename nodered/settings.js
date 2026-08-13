module.exports = {
    flowFile: "flows.json",
    flowFilePretty: true,

    uiPort: process.env.PORT || 1880,

    // Editor de flows em /admin-red, exposto via HTTPS (proxy do Nginx) e
    // protegido por login — ver docs/deploy.md.
    httpAdminRoot: "/admin-red",
    httpNodeRoot: "/",

    adminAuth: {
        type: "credentials",
        users: [
            {
                username: process.env.NODE_RED_ADMIN_USER || "admin",
                password: process.env.NODE_RED_ADMIN_PASSWORD_HASH,
                permissions: "*"
            }
        ]
    },

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

    functionGlobalContext: {
        fs: require("fs")
    },

    exportGlobalContextKeys: false,

    contextStorage: {
        default: { module: "localfilesystem" }
    }
};
