#include <git2.h>
#include <iostream>
#include <string>


int main() {
    git_libgit2_init();

    git_repository* repo;
//    git_repository_init(&repo, ".", true);

    git_repository_open(&repo, ".");

    git_reference* head;
    git_reference_lookup(&head, repo, "HEAD");


    git_signature** signature;
    git_signature_default(signature, repo);

    std::string message;
    std::cout << "Enter your message:" << '\n';
    std::cin >> message;

    git_oid tree_id, parent_id, commit_id;
    git_tree* tree;
    git_commit* parent;
    git_index* index;

    git_repository_index(&index, repo);
    git_index_write_tree(&tree_id, index);

    git_reference_name_to_id(&parent_id, repo, "HEAD");
    git_commit_lookup(&parent, repo, &parent_id);

    git_commit_create_v(&commit_id, repo, "HEAD", *signature, *signature, "utf-8", message.c_str(),
                      tree, 1, parent);

    std::cout << "Commit Id: " << commit_id.id << '\n';

    // Print the commit ID
    git_commit_free(parent);
    git_reference_free(head);
    git_repository_free(repo);
    git_libgit2_shutdown();

    return 0;
}
