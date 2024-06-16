#include <git2.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

int my_git_cred_cb(git_cred **cred, const char *url, const char *username_from_url,
                   unsigned int allowed_types, void *payload) {
    if ((allowed_types & GIT_CREDTYPE_USERPASS_PLAINTEXT) != 0) {
        return git_cred_userpass_plaintext_new(cred, "ArthurGaleev", "4529arturG");
    } else {
        return -1;
    }
    return 1;
}

// Функция для создания нового чата (ветки)
bool create_chat(const std::string& chat_name, git_repository *repo) {

    // получаем текущий указатель на HEAD
    git_reference *head_ref = nullptr;
    if (git_reference_lookup(&head_ref, repo, "refs/heads/main") != 0) {
        std::cerr << "lookup HEAD error" << git_error_last()->message << '\n';
        return false;
    }

    // получаем текущий коммит, на который указывал HEAD
    git_oid head_oid;
    git_reference_name_to_id(&head_oid, repo, "refs/heads/main");
    git_commit *head_commit = nullptr;
    if (git_commit_lookup(&head_commit, repo, &head_oid) != 0) {
        std::cerr << "lookup commit error" << git_error_last()->message << '\n';
        git_reference_free(head_ref);
        return false;
    }

    // создание новой ветки
    git_reference *branch_ref = nullptr;
    if (git_branch_create(&branch_ref, repo, chat_name.c_str(), head_commit, 0) != 0) {
        std::cerr << "branch_create error: " << git_error_last()->message << '\n';
        git_reference_free(head_ref);
        git_commit_free(head_commit);
        return false;
    }

    // Переключение на новую ветку
    const git_oid *target_oid = git_reference_target(branch_ref);
    git_commit *commit = nullptr;
    git_commit_lookup(&commit, repo, target_oid);
    git_checkout_tree(repo, (git_object *)commit, nullptr);
    git_repository_set_head(repo, git_reference_name(branch_ref));
    git_commit_free(commit);

    // Создание первого коммита
    git_signature* signature = nullptr;
    git_signature_default(&signature, repo);

    git_oid commit_id;
    git_commit_create(&commit_id, repo, chat_name.c_str(), signature, signature, "utf-8", "Chat created",
                      nullptr, 1, nullptr);
    git_signature_free(signature);

    // Обновление HEAD
    git_repository_set_head(repo, git_reference_name(branch_ref));

    // Пуш изменений на GitHub
    git_remote *remote = nullptr;
    git_remote_lookup(&remote, repo, "origin");
    git_remote_callbacks cbs;
    git_remote_init_callbacks(&cbs, GIT_REMOTE_CALLBACKS_VERSION);
    cbs.credentials = &my_git_cred_cb;
    git_push_options options;
    git_push_options_init(&options, GIT_PUSH_OPTIONS_VERSION);
    options.callbacks = cbs;

    std::string tmp = "refs/remotes/origin/" + chat_name;
    char *ctmp = new char[tmp.size() + 1];
    std::strcpy(ctmp, tmp.c_str());
    char *refspec_strings[] = {ctmp};
    git_strarray refspec_array;
    refspec_array.count = 1;
    refspec_array.strings = refspec_strings;

    git_remote_connect(remote, GIT_DIRECTION_PUSH, &cbs, nullptr, &refspec_array);
    git_remote_push(remote, &refspec_array, &options);
    git_remote_upload(remote, nullptr, &options);

    char *pathspec_strings[] = {"HEAD"};
    git_strarray pathspec_array = {pathspec_strings, 1};
    git_reset_default(repo, nullptr, &pathspec_array);
    git_remote_disconnect(remote);

    git_remote_free(remote);
    git_reference_free(head_ref);
    git_commit_free(head_commit);
    git_reference_free(branch_ref);
    delete [] ctmp;

    return true;
}

// Функция для добавления сообщения в чат
bool add_message(const std::string& chat_name, const std::string& message, git_repository *repo) {

    // Создание нового коммита с сообщением
    git_oid commit_id;
    git_signature *signature = nullptr;
    git_commit_create(&commit_id, repo, chat_name.c_str(), signature, signature, "utf-8", message.c_str(),
                      nullptr, 1, nullptr);
    git_signature_free(signature);

    // Пуш изменений на GitHub
    git_remote *remote = nullptr;
    git_remote_lookup(&remote, repo, "origin");
    git_remote_push(remote, nullptr, nullptr);
    git_remote_free(remote);

    git_repository_free(repo);
    return true;
}

// Функция для получения истории сообщений
std::vector<std::string> get_history(const std::string& chat_name, git_repository *repo) {

    std::vector<std::string> history;

    // Получение истории коммитов
    git_revwalk *walk = nullptr;
    git_revwalk_push_head(walk);
    git_revwalk_sorting(walk, GIT_SORT_TIME);

    git_oid oid;
    while (git_revwalk_next(&oid, walk) == 0) {
        git_commit *commit = nullptr;
        git_commit_lookup(&commit, repo, &oid);
        history.emplace_back(git_commit_message(commit));
        git_commit_free(commit);
    }

    git_revwalk_free(walk);
    git_repository_free(repo);
    return history;
}


int main() {
    git_libgit2_init();

    git_repository* repo = nullptr;
    int error = git_repository_open(&repo, "/home/arthur/hseprog/GitHubMessenger");
    if (error < 0) {
        const git_error *e = git_error_last();
        printf("Error %d/%d: %s\n", error, e->klass, e->message);
        exit(error);
    }

    std::cout << "Welcome to GitHubMessenger!\n"
                 "When you'll enter new or existing chat, you could use this commands by number:\n"
                 "1. Write message\n"
                 "2. Show history\n"
                 "3. Exit messenger\n\n";

    bool in_chat = false;
    std::string chat_name;
    while (true) {
        if (!in_chat) {
            std::cout << "Enter the chat name:" << '\n';
            std::cin >> chat_name;
            std::string chat_repo_url = "https://github.com/ArthurGaleev/GitHubMessengerChats/";
            chat_repo_url.append(chat_name);

            git_reference *branch_ref = nullptr;
            if (git_reference_lookup(&branch_ref, repo, ("refs/heads/" + chat_name).c_str()) != 0) {
                std::cout << "This chat does not exist. Create a new one with the same name? (yes/no)\n";
                std::string ans;
                std::cin >> ans;
                if (ans == "yes") {
                    create_chat(chat_name, repo);
                    in_chat = true;
                } else {
                    std::cout << "\n\n";
                }
            }
        } else {
            std::cout << "Enter command:" << '\n';
            int command;
            std::cin >> command;
            if (command == 1) {
                std::cout << "Enter message:" << '\n';
                std::string message;
                std::cin >> message;
                add_message(chat_name, message, repo);
            } else if (command == 2) {
                std::cout << '\n';
                for (std::string& s : get_history(chat_name, repo)) {
                    std::cout << "1. " << s << '\n';
                }
            } else if (command == 3) {
                break;
            }
        }
    }

    git_reference* head;
    git_reference_lookup(&head, repo, "HEAD");

    // возвращение HEAD обратно на main
    git_reference *main_ref = nullptr;
    git_reference_lookup(&main_ref, repo, "refs/heads/main");
    const git_oid *target_oid = git_reference_target(main_ref);
    git_commit *commit = nullptr;
    git_commit_lookup(&commit, repo, target_oid);
    git_checkout_tree(repo, (git_object *)commit, nullptr);
    git_repository_set_head(repo, git_reference_name(main_ref));
    git_commit_free(commit);

    git_reference_free(head);
    git_repository_free(repo);
    git_libgit2_shutdown();
}
