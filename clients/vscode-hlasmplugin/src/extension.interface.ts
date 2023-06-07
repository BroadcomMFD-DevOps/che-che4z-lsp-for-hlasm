import { ConfigurationProviderRegistration, HLASMExternalConfigurationProviderHandler } from "./hlasmExternalConfigurationProvider";
import { ClientInterface, ClientUriDetails } from "./hlasmExternalFiles";

export interface HlasmExtension {
    registerExternalFileClient<ConnectArgs, ReadArgs extends ClientUriDetails, ListArgs extends ClientUriDetails>(service: string, client: ClientInterface<ConnectArgs, ReadArgs, ListArgs>): void;
    registerExternalConfigurationProvider(h: HLASMExternalConfigurationProviderHandler): ConfigurationProviderRegistration;
};
