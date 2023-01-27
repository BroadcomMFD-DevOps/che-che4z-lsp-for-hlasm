/*
 * Copyright (c) 2019 Broadcom.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *   Broadcom, Inc. - initial API and implementation
 */

#ifndef HLASMPLUGIN_LANGUAGESERVER_FEATURE_LANGUAGEFEATURES_H
#define HLASMPLUGIN_LANGUAGESERVER_FEATURE_LANGUAGEFEATURES_H

#include <vector>

#include "../feature.h"
#include "../logger.h"
#include "protocol.h"
#include "workspace_manager.h"

namespace hlasm_plugin::language_server::lsp {

// a feature that implements definition, references and completion
class feature_language_features : public feature
{
public:
    feature_language_features(parser_library::workspace_manager& ws_mngr, response_provider& response_provider);

    void register_methods(std::map<std::string, method>& methods) override;
    nlohmann::json register_capabilities() override;
    void initialize_feature(const nlohmann::json& initialise_params) override;

    static nlohmann::json convert_tokens_to_num_array(const std::vector<parser_library::token_info>& tokens);

private:
    void definition(const nlohmann::json& id, const nlohmann::json& params);
    void references(const nlohmann::json& id, const nlohmann::json& params);
    void hover(const nlohmann::json& id, const nlohmann::json& params);
    void completion(const nlohmann::json& id, const nlohmann::json& params);
    void semantic_tokens(const nlohmann::json& id, const nlohmann::json& params);
    void document_symbol(const nlohmann::json& id, const nlohmann::json& params);
    void opcode_suggestion(const nlohmann::json& id, const nlohmann::json& params);

    static nlohmann::json get_markup_content(std::string_view content);
    nlohmann::json document_symbol_item_json(hlasm_plugin::parser_library::document_symbol_item symbol);
    nlohmann::json document_symbol_list_json(hlasm_plugin::parser_library::document_symbol_list symbol_list);
};

} // namespace hlasm_plugin::language_server::lsp

#endif
