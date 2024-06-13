from typing import Any, Dict
from transformers import AutoTokenizer, PreTrainedTokenizerBase

from maga_transformer.model_factory_register import register_model
from maga_transformer.config.gpt_init_model_parameters import GptInitModelParameters
from maga_transformer.models.chat_glm_v3 import ChatGlmV3

class ChatGlmV4(ChatGlmV3):
    @classmethod
    def get_tokenizer(cls, config: GptInitModelParameters) -> PreTrainedTokenizerBase:
        return AutoTokenizer.from_pretrained(config.tokenizer_path, trust_remote_code=True)
    
    @classmethod
    def update_stop_words(cls, config: GptInitModelParameters, config_json: Dict[str, Any]):
        # chatglm4 config.json is list[int], bad format
        if isinstance(config_json['eos_token_id'], list):            
            config.special_tokens.eos_token_id = config_json['eos_token_id'][0]
            config.special_tokens.stop_words_list = [[x] for x in config_json['eos_token_id']]
        else:
            config.special_tokens.eos_token_id = config_json['eos_token_id']        
    
register_model('chatglm4', ChatGlmV4, [], ["THUDM/glm4-9b-chat"])